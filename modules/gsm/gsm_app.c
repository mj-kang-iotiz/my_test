#include "gsm_app.h"
#include "FreeRTOS.h"
#include "gsm.h"
#include "gsm_port.h"
#include "log.h"
#include "lte_init.h"
#include "ntrip_app.h"
#include "timers.h"
#include <string.h>

#define TAG "GSM"

//=============================================================================
// 소켓 상태 모니터링
//=============================================================================
#define SOCKET_STATE_CHECK_INTERVAL_MS 10000  // 10초마다 상태 확인

static TimerHandle_t socket_state_timer = NULL;
static TickType_t last_recv_tick[GSM_TCP_MAX_SOCKETS] = {0};

// 소켓 상태 문자열 변환
static const char* socket_state_to_str(uint8_t state) {
  switch (state) {
    case 0: return "Initial";
    case 1: return "Opening";
    case 2: return "Connected";
    case 3: return "Listening";
    case 4: return "Closing";
    default: return "Unknown";
  }
}

// QISTATE 응답 콜백
static void socket_state_check_callback(gsm_t *gsm, gsm_cmd_t cmd, void *msg, bool is_ok) {
  if (!is_ok) {
    LOG_WARN("소켓 상태 확인 실패 (응답 없음 또는 에러)");
    return;
  }

  if (!msg || cmd != GSM_CMD_QISTATE) {
    LOG_DEBUG("소켓 상태: 활성 소켓 없음");
    return;
  }

  gsm_msg_t *m = (gsm_msg_t *)msg;

  LOG_INFO("=== 소켓 상태 [ID:%d] ===", m->qistate.connect_id);
  LOG_INFO("  상태: %s (%d)", socket_state_to_str(m->qistate.socket_state), m->qistate.socket_state);
  LOG_INFO("  서비스: %s", m->qistate.service_type);
  LOG_INFO("  원격: %s:%d", m->qistate.remote_ip, m->qistate.remote_port);
  LOG_INFO("  로컬포트: %d", m->qistate.local_port);
  LOG_INFO("  Context: %d, Access: %d", m->qistate.context_id, m->qistate.access_mode);

  // 마지막 수신 시간 확인
  uint8_t cid = m->qistate.connect_id;
  if (cid < GSM_TCP_MAX_SOCKETS && last_recv_tick[cid] != 0) {
    TickType_t now = xTaskGetTickCount();
    uint32_t elapsed_ms = (now - last_recv_tick[cid]) * portTICK_PERIOD_MS;
    LOG_INFO("  마지막 수신: %lu초 전", elapsed_ms / 1000);

    // 경고: 30초 이상 데이터 없음
    if (elapsed_ms > 30000 && m->qistate.socket_state == 2) {
      LOG_WARN("  ⚠️ 30초 이상 데이터 수신 없음!");
    }
  }
}

// 타이머 콜백 - 소켓 상태 확인 요청
static void socket_state_timer_callback(TimerHandle_t xTimer) {
  // 비동기로 상태 확인 (connect_id=0 기준)
  gsm_send_at_qistate(&gsm_handle, 1, 0, socket_state_check_callback);
}

// 소켓 상태 모니터링 시작
void gsm_socket_monitor_start(void) {
  if (socket_state_timer == NULL) {
    socket_state_timer = xTimerCreate(
      "sock_mon",
      pdMS_TO_TICKS(SOCKET_STATE_CHECK_INTERVAL_MS),
      pdTRUE,  // auto-reload
      NULL,
      socket_state_timer_callback
    );
  }

  if (socket_state_timer != NULL) {
    xTimerStart(socket_state_timer, 0);
    LOG_INFO("소켓 상태 모니터링 시작 (주기: %dms)", SOCKET_STATE_CHECK_INTERVAL_MS);
  }
}

// 소켓 상태 모니터링 중지
void gsm_socket_monitor_stop(void) {
  if (socket_state_timer != NULL) {
    xTimerStop(socket_state_timer, 0);
    LOG_INFO("소켓 상태 모니터링 중지");
  }
}

// 수신 시간 업데이트 (외부에서 호출)
void gsm_socket_update_recv_time(uint8_t connect_id) {
  if (connect_id < GSM_TCP_MAX_SOCKETS) {
    last_recv_tick[connect_id] = xTaskGetTickCount();
  }
}

/**
 * ============================================================================
 * AT 커맨드 처리 아키텍처 설명
 * ============================================================================
 *
 * 이 설계는 lwcell의 Producer-Consumer 패턴을 따릅니다.
 *
 * [3개 Task 구조]
 * 1. Caller Task: AT 명령을 요청하는 태스크 (여러 개 가능)
 * 2. Producer Task (gsm_at_cmd_process_task): 큐에서 명령을 꺼내 UART로 전송
 * 3. Consumer Task (gsm_process_task): UART 수신 데이터를 파싱
 *
 * [동기화 메커니즘]
 * - at_cmd_queue: Caller → Producer 명령 전달
 * - producer_sem: Consumer → Producer 응답 완료 시그널
 * - msg.sem: Consumer → Caller 응답 완료 시그널 (동기식 호출 시)
 * - cmd_mutex: current_cmd 접근 보호
 *
 * [실행 흐름 - 동기식 호출]
 * 1. Caller: 명령 생성 + sem 생성 → 큐에 전송 → sem 대기
 * 2. Producer: 큐에서 꺼냄 → current_cmd 설정 → UART 전송 → producer_sem 대기
 * 3. Consumer: UART 수신 → 파싱 → OK/ERROR 감지
 *              → current_cmd->sem Give (Caller 깨움)
 *              → producer_sem Give (Producer 깨움)
 * 4. Producer: 다음 명령 준비
 * 5. Caller: 응답 확인 (gsm->status.is_ok/is_err) → sem 삭제
 *
 * [실행 흐름 - 비동기식 호출]
 * Caller가 sem 대신 callback을 설정하면, Consumer가 콜백 실행
 *
 * [핵심 설계 결정]
 * Q: current_cmd_buf가 필요한가?
 * A: 아니오. Producer Task는 producer_sem을 받을 때까지 블로킹되므로
 *    at_cmd 스택 변수가 덮어써질 위험이 없음. 직접 포인터 사용 가능.
 *
 * Q: producer_sem과 msg.sem 순서는?
 * A: producer_sem을 먼저 Give하여 Producer가 다음 명령을 준비하도록 함.
 *    그 다음 msg.sem Give 또는 콜백 실행으로 Caller가 새 명령을 보낼 수 있게
 * 함.
 *
 * Q: TaskNotify 대신 세마포어를 쓰는 이유?
 * A: Producer Task는 xQueueReceive와 동시에 대기해야 함.
 *    TaskNotify와 큐를 동시에 대기하면 어느 것이 먼저 깨어날지 예측 불가.
 *
 * ============================================================================
 */

char gsm_mem[2048];
gsm_t gsm_handle;
QueueHandle_t gsm_queue;

static void gsm_process_task(void *pvParameter);
static void gsm_at_cmd_process_task(void *pvParameters);

/**
 * @brief GSM 태스크 생성
 *
 * @param arg
 */
void gsm_task_create(void *arg) {
  xTaskCreate(gsm_process_task, "gsm", 2048, arg, tskIDLE_PRIORITY + 1, NULL);
}

/**
 * ============================================================================
 * LTE 초기화 시퀀스
 * ============================================================================
 * 1. RDY 이벤트 수신 → lte_init_start() 호출
 * 2. AT 테스트 → lte_at_test_callback()
 * 3. AT+CMEE=2 설정 → lte_cmee_set_callback()
 * 4. AT+CPIN? SIM 확인 → lte_cpin_check_callback()
 * 5. AT+CGDCONT APN 설정 → lte_apn_set_callback()
 * 6. AT+CGDCONT? APN 확인 → lte_apn_verify_callback()
 * 7. AT+COPS? 네트워크 등록 확인 → lte_network_check_callback()
 * 8. GSM_EVT_INIT_OK 이벤트 발생
 *
 * [재시도 메커니즘]
 * - 각 단계 실패 시 최대 3회 소프트웨어 재시도
 * - 3회 실패 후 EC25 모듈 하드웨어 리셋 + 1회 추가 시도
 * - 네트워크 등록은 6초마다 최대 20회 체크 (약 2분)
 * - 최종 실패 시 GSM_EVT_INIT_FAIL 이벤트 발생 (상위 레이어 판단)
 * ============================================================================
 */

static void gsm_evt_handler(gsm_evt_t evt, void *args) {
  switch (evt) {
  case GSM_EVT_RDY: {
    LOG_INFO("RDY 신호 수신");

    // LTE 초기화 시작
    if (lte_get_init_state() == LTE_INIT_IDLE) {
      // 첫 시작 또는 하드웨어 리셋 후
      if (lte_get_retry_count() == 0) {
        // 첫 시작: 카운터 초기화
        LOG_INFO("첫 번째 LTE 초기화 시작");
      } else if (lte_get_retry_count() == LTE_INIT_MAX_RETRY + 1) {
        // 하드웨어 리셋 후: 카운터 유지
        LOG_INFO("하드웨어 리셋 후 LTE 초기화 재시작");
      }

      lte_init_start();
    }
    break;
  }

  case GSM_EVT_INIT_OK: {
    LOG_INFO("애플리케이션: LTE 초기화 성공");
    // 여기서 추가 작업 수행 가능 (예: TCP 연결 등)
    ntrip_task_create(&gsm_handle);
    break;
  }

  case GSM_EVT_INIT_FAIL: {
    LOG_ERR("애플리케이션: LTE 초기화 실패");
    // 여기서 재시도 로직 등 구현 가능
    break;
  }

  default:
    break;
  }
}

/**
 * @brief GSM 태스크
 *
 * @param pvParameter
 */
static void gsm_process_task(void *pvParameter) {
  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;
  size_t total_received = 0;

  gsm_queue = xQueueCreate(10, 1);

  // 네트워크 체크 타이머 생성 (한 번만, 재사용)
  TimerHandle_t network_timer =
      xTimerCreate("lte_net_chk", pdMS_TO_TICKS(LTE_NETWORK_CHECK_INTERVAL_MS),
                   pdFALSE, // one-shot
                   NULL, lte_network_check_timer_callback);

  gsm_init(&gsm_handle, gsm_evt_handler, NULL);
  gsm_port_init();
  gsm_start();

  // LTE 초기화 모듈 설정
  lte_set_gsm_handle(&gsm_handle);
  lte_set_network_check_timer(network_timer);

  // AT 커맨드 처리 태스크 생성
  xTaskCreate(gsm_at_cmd_process_task, "gsm_at_cmd", 2048, &gsm_handle,
              tskIDLE_PRIORITY + 2, NULL);

  while (1) {
    xQueueReceive(gsm_queue, &dummy, portMAX_DELAY);
    pos = gsm_get_rx_pos();

    if (pos != old_pos) {
      if (pos > old_pos) {
        size_t len = pos - old_pos;
        total_received = len;
        LOG_DEBUG("RX: %u bytes", len);
        LOG_DEBUG_RAW("RAW: ", &gsm_mem[old_pos], len);
        gsm_parse_process(&gsm_handle, &gsm_mem[old_pos], pos - old_pos);
      } else {
        size_t len1 = sizeof(gsm_mem) - old_pos;
        size_t len2 = pos;
        total_received = len1 + len2;
        LOG_DEBUG("RX: %u bytes (wrapped: %u+%u)", total_received, len1, len2);

        LOG_DEBUG_RAW("RAW: ", &gsm_mem[old_pos], len1);
        gsm_parse_process(&gsm_handle, &gsm_mem[old_pos],
                          sizeof(gsm_mem) - old_pos);
        if (pos > 0) {
          LOG_DEBUG_RAW("RAW: ", gsm_mem, len2);

          gsm_parse_process(&gsm_handle, gsm_mem, pos);
        }
      }
      old_pos = pos;
      if (old_pos == sizeof(gsm_mem)) {
        old_pos = 0;
      }
    }
  }

  vTaskDelete(NULL);
}

static void gsm_at_cmd_process_task(void *pvParameters) {
  gsm_t *gsm = (gsm_t *)pvParameters;
  gsm_at_cmd_t at_cmd;

  while (1) {
    if (xQueueReceive(gsm->at_cmd_queue, &at_cmd, portMAX_DELAY) == pdTRUE) {
      // 1. 수신 받은 커맨드가 정상인지 확인
      if ((at_cmd.cmd >= GSM_CMD_MAX) || (at_cmd.cmd == GSM_CMD_NONE)) {
        if (at_cmd.sem != NULL) {
          xSemaphoreGive(at_cmd.sem);
          vSemaphoreDelete(at_cmd.sem);
        }

        continue;
      }

      // 2. current_cmd 설정 (스택 변수를 직접 가리킴)
      // ★ 중요: AT 명령 전송 전에 current_cmd를 먼저 설정해야 함
      //          빠른 응답 수신 시 current_cmd가 NULL이면 응답을 놓칠 수 있음
      // ★ 안전성: Producer Task는 아래 producer_sem을 받을 때까지 블로킹되므로
      //           at_cmd 스택 변수가 덮어써질 위험이 없음 → 직접 포인터 사용
      //           가능
      if (xSemaphoreTake(gsm->cmd_mutex, portMAX_DELAY) == pdTRUE) {
        gsm->current_cmd = &at_cmd;

        // ★ lwcell 방식: msg union 초기화
        memset(&gsm->current_cmd->msg, 0, sizeof(gsm->current_cmd->msg));

        xSemaphoreGive(gsm->cmd_mutex);
      }

      // 3. at 커맨드 전송
      const char *at_mode = NULL;

      switch (at_cmd.at_mode) {
      case GSM_AT_EXECUTE:
        break;

      case GSM_AT_WRITE:
        at_mode = "=";
        break;

      case GSM_AT_READ:
        at_mode = "?";
        break;

      case GSM_AT_TEST:
        at_mode = "=?";
        break;

      default:
        break;
      }

      gsm->ops->send(gsm->at_tbl[at_cmd.cmd].at_str,
                     strlen(gsm->at_tbl[at_cmd.cmd].at_str));

      if (at_mode != NULL) {
        gsm->ops->send(at_mode, strlen(at_mode));
      }

      if (at_cmd.params[0] != '\0') {
        gsm->ops->send(at_cmd.params, strlen(at_cmd.params));
      }
      gsm->ops->send("\r\n", 2);

      // ★★★ lwcell 방식: Producer Task가 응답 대기 ★★★
      // - 동기/비동기 관계없이 Producer는 항상 응답을 기다림
      // - 다음 명령 순차 처리 보장 (응답 섞임 방지)
      // - 10ms 폴링 대신 세마포어로 효율적 대기

      uint32_t timeout_ms = gsm->at_tbl[at_cmd.cmd].timeout_ms;
      if (timeout_ms == 0) {
        timeout_ms = 5000; // 기본 타임아웃 5초
      }

      TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
      TickType_t start_tick = xTaskGetTickCount();

      // ★★★ Producer Task 응답 대기 ★★★
      // - 응답이 올 때까지 여기서 블로킹
      // - 블로킹 중에는 at_cmd 스택 변수가 보존됨 (덮어써질 위험 없음)
      // - 타임아웃 또는 응답 수신 시 다음 루프로 진행
      // - UART 전송 후 경과 시간을 고려하여 정확한 타임아웃 적용
      TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
      TickType_t remaining_ticks =
          (timeout_ticks > elapsed_ticks) ? (timeout_ticks - elapsed_ticks) : 0;

      if (xSemaphoreTake(gsm->producer_sem, remaining_ticks) != pdTRUE) {
        // ★ 타임아웃 발생: 명령 응답 없음
        if (xSemaphoreTake(gsm->cmd_mutex, portMAX_DELAY) == pdTRUE) {
          // 현재 명령이 우리가 보낸 명령인지 확인 (race condition 방지)
          if (gsm->current_cmd == &at_cmd) {
            gsm->current_cmd = NULL;

            // ★ 타임아웃 상태 설정 (OK/ERROR 체크 가능하도록)
            gsm->status.is_ok = 0;
            gsm->status.is_err = 1;

            // Caller가 대기 중이면 깨워줌 (동기식인 경우)
            if (at_cmd.sem) {
              xSemaphoreGive(at_cmd.sem);
            }
            // 비동기식이면 콜백 실행 (에러 처리)
            else if (at_cmd.callback) {
              at_cmd.callback(gsm, at_cmd.cmd, NULL, false);
            }

            // ★ TX pbuf 해제 (타임아웃 시에도 메모리 누수 방지)
            if (at_cmd.tx_pbuf) {
              tcp_pbuf_free(at_cmd.tx_pbuf);
            }
          }
          xSemaphoreGive(gsm->cmd_mutex);
        }
      }

      // 응답 완료 (또는 타임아웃)
      // 다음 명령 처리 가능
    }
  }

  vTaskDelete(NULL);
}
