#ifndef NTRIP_APP_H
#define NTRIP_APP_H

#include "gsm.h"
#include "tcp_socket.h"

/**
 * @brief NTRIP TCP 수신 태스크 생성
 *
 * @param gsm GSM 핸들
 */
void ntrip_task_create(gsm_t *gsm);

/**
 * @brief NTRIP 소켓 포인터 가져오기 (GPS에서 GGA 전송용)
 *
 * @return NTRIP 소켓 포인터 (NULL이면 연결 안됨)
 */
tcp_socket_t* ntrip_get_socket(void);

#endif // NTRIP_TASK_H