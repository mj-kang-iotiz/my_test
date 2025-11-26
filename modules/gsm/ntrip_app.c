#include "ntrip_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tcp_socket.h"
#include "gps_app.h"
#include <string.h>
#include "led.h"

#ifndef TAG
#define TAG "NTRIP"
#endif

#include "log.h"

// NTRIP 서버 정보
#define NTRIP_SERVER_IP "210.117.198.84"
// #define NTRIP_SERVER_IP "time.nist.gov"
#define NTRIP_SERVER_PORT 2101
// #define NTRIP_SERVER_PORT 13
#define NTRIP_CONNECT_ID 0 // 소켓 ID (0-11)
#define NTRIP_CONTEXT_ID 1 // PDP context ID

#define NTRIP_MAX_CONNECT_RETRY 3
#define NTRIP_MAX_TIMEOUT_COUNT 3  // 연속 타임아웃 최대 허용 횟수
#define NTRIP_RECONNECT_DELAY_MS 2000  // 재연결 대기 시간 (ms)

// NTRIP HTTP 요청
static const char NTRIP_HTTP_REQUEST[] =
    "GET /SONP-RTCM32 HTTP/1.0\r\n"
    "User-Agent: NTRIP GUGU SYSTEM\r\n"
    "Accept: */*\r\n"
    "Connection: close\r\n"
    "Authorization: Basic bWoua2FuZ0Bpb3Rpei5rcjpnbnNz\r\n"
    "\r\n";

uint8_t recv_buf[1500];

// NTRIP 소켓 전역 포인터 (GGA 전송용)
static tcp_socket_t *g_ntrip_socket = NULL;

/**
 * @brief NTRIP 소켓 포인터 가져오기 (GGA 전송용)
 * @return NTRIP 소켓 포인터 (NULL: 연결 안됨)
 */
tcp_socket_t* ntrip_get_socket(void) {
  return g_ntrip_socket;
}

static int ntrip_connect_to_server(tcp_socket_t *sock) {
  int ret;
  int retry_count = 0;

  while (retry_count < NTRIP_MAX_CONNECT_RETRY) {
    LOG_INFO("NTRIP 서버 연결 시도 [%d/%d]: %s:%d",
             retry_count + 1, NTRIP_MAX_CONNECT_RETRY,
             NTRIP_SERVER_IP, NTRIP_SERVER_PORT);

    ret = tcp_connect(sock, NTRIP_CONTEXT_ID, NTRIP_SERVER_IP, NTRIP_SERVER_PORT,
                      10000);

    if (ret == 0 && tcp_get_socket_state(sock, NTRIP_CONNECT_ID) == GSM_TCP_STATE_CONNECTED) {
      LOG_INFO("TCP 연결 성공");

      // HTTP 요청 전송
      LOG_INFO("NTRIP HTTP 요청 전송");
      ret = tcp_send(sock, (const uint8_t *)NTRIP_HTTP_REQUEST,
                     strlen(NTRIP_HTTP_REQUEST));

      if (ret < 0) {
        LOG_ERR("HTTP 요청 전송 실패: %d", ret);
        tcp_close_force(sock);
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
        continue;
      }

      LOG_INFO("HTTP 요청 전송 완료 (%d bytes)", ret);

      // 수신 타임아웃 설정 (10초)
      tcp_set_recv_timeout(sock, 10000);

      // ICY 200 OK 수신
      ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);
      if (ret > 0) {
        LOG_INFO("서버 응답 수신 (%d bytes)", ret);
        return 0;  // 연결 성공
      }
    }

    // 연결 실패 - 강제 닫기 후 재시도
    LOG_WARN("TCP 연결 실패 (ret=%d), 강제 닫기 후 재시도...", ret);
    led_set_color(LED_ID_1, LED_COLOR_YELLOW);
    tcp_close_force(sock);

    // 재시도 전 대기
    vTaskDelay(pdMS_TO_TICKS(1000));
    retry_count++;
  }

 

  LOG_ERR("TCP 연결 최대 재시도 횟수 초과");

  return -1;

}

/**
 * @brief NTRIP TCP 수신 태스크
 */
static void ntrip_tcp_recv_task(void *pvParameter) {
  gsm_t *gsm = (gsm_t *)pvParameter;
  tcp_socket_t *sock = NULL;
  gps_t* gps_handle = gps_get_instance_handle(0);

  int ret;
  int timeout_count = 0;  // 연속 타임아웃 카운터
  int reconnect_count = 0;  // 총 재연결 시도 횟수

  LOG_INFO("NTRIP 태스크 시작");

  // TCP 소켓 생성
  sock = tcp_socket_create(gsm, NTRIP_CONNECT_ID);
  if (!sock) {
    LOG_ERR("TCP 소켓 생성 실패");
    vTaskDelete(NULL);
    return;
  }
  LOG_INFO("TCP 소켓 생성 완료");

  // 전역 소켓 포인터 설정 (GPS에서 GGA 전송용)
  g_ntrip_socket = sock;
  
  if (ntrip_connect_to_server(sock) != 0) {
    LOG_ERR("초기 연결 실패");
    g_ntrip_socket = NULL;
    tcp_socket_destroy(sock);
    vTaskDelete(NULL);
    return;
  }

  gsm_socket_monitor_start();

  // HTTP 요청 전송 (한 번만)
  LOG_INFO("NTRIP HTTP 요청 전송");
  ret = tcp_send(sock, (const uint8_t *)NTRIP_HTTP_REQUEST,
                 strlen(NTRIP_HTTP_REQUEST));
  if (ret < 0) {
    LOG_ERR("HTTP 요청 전송 실패: %d", ret);
    g_ntrip_socket = NULL;
    tcp_close(sock);
    tcp_socket_destroy(sock);
    vTaskDelete(NULL);
    return;
  }

  LOG_INFO("HTTP 요청 전송 완료 (%d bytes)", ret);

  // ICY 200 OK\r\n\r\n 수신
  ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);
  led_set_color(1, LED_COLOR_GREEN);

  // 무한 루프: 데이터 수신
  while (1) {
    // 데이터 수신 (블로킹)
    ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);

    if (ret > 0) {
      // 수신 성공
      timeout_count = 0;
      LOG_INFO("수신 데이터 (%d bytes):", ret);

      // 데이터를 16진수로 출력
      for (int i = 0; i < ret; i += 16) {
        char hex_str[64] = {0};
        char ascii_str[20] = {0};
        int line_len = (ret - i) > 16 ? 16 : (ret - i);

        // 16진수 문자열 생성
        for (int j = 0; j < line_len; j++) {
          sprintf(&hex_str[j * 3], "%02X ", recv_buf[i + j]);

          // ASCII 출력용 (출력 가능한 문자만)
          if (recv_buf[i + j] >= 0x20 && recv_buf[i + j] <= 0x7E) {
            ascii_str[j] = recv_buf[i + j];
          } else {
            ascii_str[j] = '.';
          }
        }

        LOG_INFO("  %04X: %-48s | %s", i, hex_str, ascii_str);
      }
      gps_handle->ops->send((const char*)recv_buf, ret);
    } else if (ret == 0) {
      // 타임아웃
      timeout_count++;
      LOG_WARN("수신 타임아웃 (%d/%d)", timeout_count, NTRIP_MAX_TIMEOUT_COUNT);

      // 소켓 상태 확인
      gsm_tcp_state_t state = tcp_get_socket_state(sock, NTRIP_CONNECT_ID);
      if (state == GSM_TCP_STATE_CLOSING || state == GSM_TCP_STATE_CLOSED) {
        LOG_ERR("소켓 상태 비정상 (state=%d), 재연결 필요", state);
        timeout_count = NTRIP_MAX_TIMEOUT_COUNT;  // 즉시 재연결
      }

      // 연속 타임아웃 최대 횟수 초과 시 재연결
      if (timeout_count >= NTRIP_MAX_TIMEOUT_COUNT) {
        LOG_WARN("연속 타임아웃 발생 또는 소켓 끊김, 재연결 시도...");
        led_set_color(1, LED_COLOR_YELLOW);
        // 기존 연결 닫기
        tcp_close_force(sock);
        vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS));

        // 재연결 시도
        if (ntrip_connect_to_server(sock) != 0) {
          reconnect_count++;
          LOG_ERR("재연결 실패 (%d회)", reconnect_count);

          // 재연결 실패 시 더 긴 대기
          vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS * 2));
        } else {
          LOG_INFO("재연결 성공");
          led_set_color(1, LED_COLOR_GREEN);
          timeout_count = 0;  // 타임아웃 카운터 리셋
        }
      }
    } else {
      // 에러
      LOG_ERR("수신 에러: %d", ret);

      // 소켓 상태 확인
      gsm_tcp_state_t state = tcp_get_socket_state(sock, NTRIP_CONNECT_ID);
      LOG_ERR("현재 소켓 상태: %d", state);

      // 에러 발생 시 재연결 시도
      LOG_WARN("에러 발생, 재연결 시도...");
      led_set_color(1, LED_COLOR_YELLOW);
      tcp_close_force(sock);
      vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS));

      if (ntrip_connect_to_server(sock) != 0) {
        reconnect_count++;
        LOG_ERR("재연결 실패 (%d회)", reconnect_count);
        vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS * 2));
      } else {
        LOG_INFO("재연결 성공");
        timeout_count = 0;
        led_set_color(1, LED_COLOR_GREEN);
      }
    }
  }

  // 연결 종료
  LOG_INFO("TCP 연결 종료");
  g_ntrip_socket = NULL;
  tcp_close(sock);
  tcp_socket_destroy(sock);

  vTaskDelete(NULL);
}

void ntrip_task_create(gsm_t *gsm) {
  xTaskCreate(ntrip_tcp_recv_task, "ntrip_recv", 2048, gsm,
              tskIDLE_PRIORITY + 3, NULL);
}
