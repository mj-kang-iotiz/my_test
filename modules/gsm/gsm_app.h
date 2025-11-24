#ifndef GSM_APP_H
#define GSM_APP_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <stdint.h>

extern QueueHandle_t gsm_queue;

void gsm_task_create(void *arg);

//=============================================================================
// 소켓 상태 모니터링 API
//=============================================================================

/**
 * @brief 소켓 상태 모니터링 시작
 *        주기적으로 AT+QISTATE 명령으로 소켓 상태를 확인하고 로그 출력
 */
void gsm_socket_monitor_start(void);

/**
 * @brief 소켓 상태 모니터링 중지
 */
void gsm_socket_monitor_stop(void);

/**
 * @brief 소켓 수신 시간 업데이트
 *        데이터 수신 시 호출하여 마지막 수신 시간 기록
 * @param connect_id 소켓 ID
 */
void gsm_socket_update_recv_time(uint8_t connect_id);

#endif
