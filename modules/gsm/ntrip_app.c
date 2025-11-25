#include "ntrip_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tcp_socket.h"
#include "gps_app.h"
#include <string.h>

#ifndef TAG
#define TAG "NTRIP"
#endif

#include "log.h"

// NTRIP 태스크 핸들 (중지용)
static TaskHandle_t ntrip_task_handle = NULL;
static volatile bool ntrip_task_stop_requested = false;

// NTRIP 서버 정보
#define NTRIP_SERVER_IP "210.117.198.84"
// #define NTRIP_SERVER_IP "time.nist.gov"
#define NTRIP_SERVER_PORT 2101
// #define NTRIP_SERVER_PORT 13
#define NTRIP_CONNECT_ID 0 // 소켓 ID (0-11)
#define NTRIP_CONTEXT_ID 1 // PDP context ID

// NTRIP HTTP 요청
static const char NTRIP_HTTP_REQUEST[] =
    "GET /SONP-RTCM32 HTTP/1.0\r\n"
    "User-Agent: GUGU SYSTEM RTK\r\n"
    "Accept: */*\r\n"
    "Connection: close\r\n"
    "Authorization: Basic bWoua2FuZ0Bpb3Rpei5rcjpnbnNz\r\n"
    "\r\n";

uint8_t recv_buf[1500];

/**
 * @brief NTRIP TCP 수신 태스크
 *
 * 연결 실패 또는 종료 시 자동으로 재연결 시도
 * 지수 백오프 전략 사용 (1초 → 2초 → 4초 → ... → 최대 60초)
 */
static void ntrip_tcp_recv_task(void *pvParameter) {
  gsm_t *gsm = (gsm_t *)pvParameter;
  tcp_socket_t *sock = NULL;
  gps_t* gps_handle = gps_get_handle();
  int ret;
  uint32_t reconnect_delay = NTRIP_RECONNECT_DELAY_INITIAL;

  LOG_INFO("NTRIP 태스크 시작 (자동 재연결 활성화)");

  // ★ 무한 루프: 연결 실패/종료 시 재시도
  while (!ntrip_task_stop_requested) {

    // ============================================================
    // 1. TCP 소켓 생성
    // ============================================================
    sock = tcp_socket_create(gsm, NTRIP_CONNECT_ID);
    if (!sock) {
      LOG_ERR("TCP 소켓 생성 실패 - %d초 후 재시도", reconnect_delay / 1000);
      vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
      reconnect_delay = (reconnect_delay * NTRIP_RECONNECT_BACKOFF_MULTIPLIER);
      if (reconnect_delay > NTRIP_RECONNECT_DELAY_MAX) {
        reconnect_delay = NTRIP_RECONNECT_DELAY_MAX;
      }
      continue; // 재시도
    }

    LOG_INFO("TCP 소켓 생성 완료");

    // ============================================================
    // 2. NTRIP 서버 연결
    // ============================================================
    LOG_INFO("NTRIP 서버 연결 시도: %s:%d", NTRIP_SERVER_IP, NTRIP_SERVER_PORT);
    ret = tcp_connect(sock, NTRIP_CONTEXT_ID, NTRIP_SERVER_IP,
                      NTRIP_SERVER_PORT, 10000);
    if (ret != 0) {
      LOG_ERR("TCP 연결 실패: %d - %d초 후 재시도", ret, reconnect_delay / 1000);
      tcp_socket_destroy(sock);
      sock = NULL;
      vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
      reconnect_delay = (reconnect_delay * NTRIP_RECONNECT_BACKOFF_MULTIPLIER);
      if (reconnect_delay > NTRIP_RECONNECT_DELAY_MAX) {
        reconnect_delay = NTRIP_RECONNECT_DELAY_MAX;
      }
      continue; // 재시도
    }

    // 연결 상태 확인
    if (tcp_get_socket_state(sock, 0) != GSM_TCP_STATE_CONNECTED) {
      LOG_WARN("TCP 연결 상태 불일치 - 재연결 시도");
      tcp_close(sock);
      ret = tcp_connect(sock, NTRIP_CONTEXT_ID, NTRIP_SERVER_IP,
                        NTRIP_SERVER_PORT, 10000);
      if (ret != 0) {
        LOG_ERR("TCP 재연결 실패: %d - %d초 후 재시도", ret,
                reconnect_delay / 1000);
        tcp_socket_destroy(sock);
        sock = NULL;
        vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
        reconnect_delay = (reconnect_delay * NTRIP_RECONNECT_BACKOFF_MULTIPLIER);
        if (reconnect_delay > NTRIP_RECONNECT_DELAY_MAX) {
          reconnect_delay = NTRIP_RECONNECT_DELAY_MAX;
        }
        continue; // 재시도
      }
    }

    LOG_INFO("TCP 연결 성공");

    // ============================================================
    // 3. TCP keep-alive 설정 (전역 설정, 모든 소켓에 적용)
    // ============================================================
    LOG_INFO("TCP keep-alive 설정 (전역)");
    gsm_send_at_qicfg_keepalive(gsm, 1, 60, 10, 3, NULL);

    if (gsm->status.is_err || gsm->status.is_timeout) {
      LOG_WARN("TCP keep-alive 설정 실패 (무시하고 계속)");
    } else {
      LOG_INFO("TCP keep-alive 설정 완료");
    }

    // ============================================================
    // 4. NTRIP HTTP 요청 전송
    // ============================================================
    LOG_INFO("NTRIP HTTP 요청 전송");
    ret = tcp_send(sock, (const uint8_t *)NTRIP_HTTP_REQUEST,
                   strlen(NTRIP_HTTP_REQUEST));
    if (ret < 0) {
      LOG_ERR("HTTP 요청 전송 실패: %d - %d초 후 재시도", ret,
              reconnect_delay / 1000);
      tcp_close(sock);
      tcp_socket_destroy(sock);
      sock = NULL;
      vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
      reconnect_delay = (reconnect_delay * NTRIP_RECONNECT_BACKOFF_MULTIPLIER);
      if (reconnect_delay > NTRIP_RECONNECT_DELAY_MAX) {
        reconnect_delay = NTRIP_RECONNECT_DELAY_MAX;
      }
      continue; // 재시도
    }

    LOG_INFO("HTTP 요청 전송 완료 (%d bytes)", ret);

    // 수신 타임아웃 설정 (10초)
    tcp_set_recv_timeout(sock, 10000);

    // ICY 200 OK\r\n\r\n 수신
    ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);
    if (ret < 0) {
      LOG_ERR("ICY 응답 수신 실패: %d - %d초 후 재시도", ret,
              reconnect_delay / 1000);
      tcp_close(sock);
      tcp_socket_destroy(sock);
      sock = NULL;
      vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
      reconnect_delay = (reconnect_delay * NTRIP_RECONNECT_BACKOFF_MULTIPLIER);
      if (reconnect_delay > NTRIP_RECONNECT_DELAY_MAX) {
        reconnect_delay = NTRIP_RECONNECT_DELAY_MAX;
      }
      continue; // 재시도
    }

    LOG_INFO("NTRIP 서버 응답 수신 성공");

    // ★ 연결 성공 - 백오프 리셋
    reconnect_delay = NTRIP_RECONNECT_DELAY_INITIAL;

    // ============================================================
    // 5. 데이터 수신 루프
    // ============================================================
    LOG_INFO("NTRIP 데이터 수신 시작");
    while (!ntrip_task_stop_requested) {
      // 데이터 수신 (블로킹, 10초 타임아웃)
      ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);

      if (ret > 0) {
        // ★ GPS로 즉시 전송 (오버플로우 방지)
        gps_handle->ops->send((const char *)recv_buf, ret);

#if NTRIP_DEBUG_HEXDUMP
        // 디버그 모드: 16진수 덤프 (프로덕션에서는 비활성화 권장)
        LOG_INFO("NTRIP RX (%d bytes):", ret);
        for (int i = 0; i < ret; i += 16) {
          char hex_str[64] = {0};
          char ascii_str[20] = {0};
          int line_len = (ret - i) > 16 ? 16 : (ret - i);

          for (int j = 0; j < line_len; j++) {
            sprintf(&hex_str[j * 3], "%02X ", recv_buf[i + j]);
            if (recv_buf[i + j] >= 0x20 && recv_buf[i + j] <= 0x7E) {
              ascii_str[j] = recv_buf[i + j];
            } else {
              ascii_str[j] = '.';
            }
          }
          LOG_INFO("  %04X: %-48s | %s", i, hex_str, ascii_str);
        }
#else
        // 프로덕션 모드: 간단한 로그만
        LOG_DEBUG("NTRIP RX: %d bytes", ret);
#endif

      } else if (ret == 0) {
        // 타임아웃 (10초 동안 데이터 없음)
        LOG_DEBUG("수신 타임아웃 (정상 - keep-alive 동작 중)");
        // 타임아웃은 정상 동작 (keep-alive가 연결 상태 확인)
      } else {
        // 에러 발생 - 연결 종료 또는 네트워크 문제
        LOG_ERR("수신 에러: %d - 연결 종료 감지", ret);
        break; // 내부 루프 탈출 → 재연결
      }
    }

    // ============================================================
    // 6. 연결 종료 및 정리
    // ============================================================
    LOG_INFO("TCP 연결 종료 (재연결 준비 중)");
    if (sock) {
      tcp_close(sock);
      tcp_socket_destroy(sock);
      sock = NULL;
    }

    // 재연결 대기
    if (!ntrip_task_stop_requested) {
      LOG_INFO("%d초 후 재연결 시도", reconnect_delay / 1000);
      vTaskDelay(pdMS_TO_TICKS(reconnect_delay));
      reconnect_delay = (reconnect_delay * NTRIP_RECONNECT_BACKOFF_MULTIPLIER);
      if (reconnect_delay > NTRIP_RECONNECT_DELAY_MAX) {
        reconnect_delay = NTRIP_RECONNECT_DELAY_MAX;
      }
    }
  } // while (!ntrip_task_stop_requested)

  // 태스크 종료
  LOG_INFO("NTRIP 태스크 종료");
  ntrip_task_handle = NULL;
  vTaskDelete(NULL);
}

void ntrip_task_create(gsm_t *gsm) {
  if (ntrip_task_handle != NULL) {
    LOG_WARN("NTRIP 태스크가 이미 실행 중입니다");
    return;
  }

  ntrip_task_stop_requested = false;
  xTaskCreate(ntrip_tcp_recv_task, "ntrip_recv", 2048, gsm,
              tskIDLE_PRIORITY + 3, &ntrip_task_handle);
  LOG_INFO("NTRIP 태스크 생성 완료");
}

void ntrip_task_stop(void) {
  if (ntrip_task_handle == NULL) {
    LOG_WARN("NTRIP 태스크가 실행 중이지 않습니다");
    return;
  }

  LOG_INFO("NTRIP 태스크 중지 요청");
  ntrip_task_stop_requested = true;

  // 태스크가 종료될 때까지 대기 (최대 5초)
  for (int i = 0; i < 50; i++) {
    if (ntrip_task_handle == NULL) {
      LOG_INFO("NTRIP 태스크 중지 완료");
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  LOG_WARN("NTRIP 태스크 중지 타임아웃 (강제 종료)");
}
