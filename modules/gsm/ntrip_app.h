#ifndef NTRIP_APP_H
#define NTRIP_APP_H

#include "gsm.h"

// NTRIP 재연결 설정
#define NTRIP_RECONNECT_DELAY_INITIAL 1000  // 초기 재연결 대기 시간 (1초)
#define NTRIP_RECONNECT_DELAY_MAX 60000     // 최대 재연결 대기 시간 (60초)
#define NTRIP_RECONNECT_BACKOFF_MULTIPLIER 2 // 백오프 배율 (지수 백오프)

/**
 * @brief NTRIP TCP 수신 태스크 생성
 *
 * @param gsm GSM 핸들
 */
void ntrip_task_create(gsm_t *gsm);

/**
 * @brief NTRIP 태스크 중지
 */
void ntrip_task_stop(void);

#endif // NTRIP_TASK_H