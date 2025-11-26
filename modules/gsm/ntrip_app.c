#include "ntrip_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tcp_socket.h"
#include "gps_app.h"
#include "led.h"
#include <string.h>

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

// NTRIP HTTP 요청
static const char NTRIP_HTTP_REQUEST[] =
    "GET /SONP-RTCM32 HTTP/1.0\r\n"
    "User-Agent: NTRIP GUGU SYSTEM\r\n"
    "Accept: */*\r\n"
    "Connection: close\r\n"
    "Authorization: Basic bWoua2FuZ0Bpb3Rpei5rcjpnbnNz\r\n"
    "\r\n";

uint8_t recv_buf[1500];
/**
 * @brief NTRIP TCP 수신 태스크
 */
static void ntrip_tcp_recv_task(void *pvParameter) {
  gsm_t *gsm = (gsm_t *)pvParameter;
  tcp_socket_t *sock = NULL;
  gps_t* gps_handle = gps_get_handle();

  int ret;
  int retry_count = 0;

  LOG_INFO("NTRIP 태스크 시작");

  // TCP 소켓 생성
  sock = tcp_socket_create(gsm, NTRIP_CONNECT_ID);
  if (!sock) {
    LOG_ERR("TCP 소켓 생성 실패");
    vTaskDelete(NULL);
    return;
  }
  LOG_INFO("TCP 소켓 생성 완료");
    while (retry_count < NTRIP_MAX_CONNECT_RETRY) {

    LOG_INFO("NTRIP 서버 연결 시도 [%d/%d]: %s:%d",
             retry_count + 1, NTRIP_MAX_CONNECT_RETRY,
             NTRIP_SERVER_IP, NTRIP_SERVER_PORT);


    ret = tcp_connect(sock, NTRIP_CONTEXT_ID, NTRIP_SERVER_IP, NTRIP_SERVER_PORT,
                      10000);

    if (ret == 0 && tcp_get_socket_state(sock, NTRIP_CONNECT_ID) == GSM_TCP_STATE_CONNECTED) {
      LOG_INFO("TCP 연결 성공");
      led_set_color(LED_ID_1, LED_COLOR_GREEN);  // ✅ 초록색 - 정상 연결
      break;  // 연결 성공
    }

    // ★ 연결 실패 - 강제 닫기 후 재시도
    LOG_WARN("TCP 연결 실패 (ret=%d), 강제 닫기 후 재시도...", ret);
    led_set_color(LED_ID_1, LED_COLOR_YELLOW);  // ⚠️ 노란색 - 재시도 중
    tcp_close_force(sock);

    // 재시도 전 대기 (1초)
    vTaskDelay(pdMS_TO_TICKS(1000));
    retry_count++;
  }

  if (retry_count >= NTRIP_MAX_CONNECT_RETRY) {
    LOG_ERR("TCP 연결 최대 재시도 횟수 초과");
    led_set_color(LED_ID_1, LED_COLOR_RED);  // ❌ 빨간색 - 연결 실패
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
    tcp_close(sock);
    tcp_socket_destroy(sock);
    vTaskDelete(NULL);
    return;
  }

  LOG_INFO("HTTP 요청 전송 완료 (%d bytes)", ret);

  // 수신 타임아웃 설정 (10초)
  tcp_set_recv_timeout(sock, 10000);

  // ICY 200 OK\r\n\r\n 수신
  ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);

  // 무한 루프: 데이터 수신
  while (1) {
    // 데이터 수신 (블로킹)
    ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);

    if (ret > 0) {
      // 수신 성공
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
      LOG_INFO("수신 타임아웃 (10초 동안 데이터 없음)");
    } else {
      // 에러
      LOG_ERR("수신 에러: %d", ret);
      led_set_color(LED_ID_1, LED_COLOR_YELLOW);  // ⚠️ 노란색 - 수신 오류
      break;
    }
  }

  // 연결 종료
  LOG_INFO("TCP 연결 종료");
  tcp_close(sock);
  tcp_socket_destroy(sock);

  vTaskDelete(NULL);
}

void ntrip_task_create(gsm_t *gsm) {
  xTaskCreate(ntrip_tcp_recv_task, "ntrip_recv", 2048, gsm,
              tskIDLE_PRIORITY + 3, NULL);
}
