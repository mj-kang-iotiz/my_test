#ifndef NTRIP_APP_H
#define NTRIP_APP_H

#include "gsm.h"

/**
 * @brief NTRIP TCP 수신 태스크 생성
 *
 * @param gsm GSM 핸들
 */
void ntrip_task_create(gsm_t *gsm);

#endif // NTRIP_TASK_H