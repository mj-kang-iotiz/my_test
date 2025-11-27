#ifndef GPS_APP_H
#define GPS_APP_H

#include "gps.h"
#include "board_config.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/**
 * @brief GPS 초기화 (board_config 기반)
 *
 * board_config.h의 설정을 읽어서 자동으로 GPS 인스턴스 생성
 */
void gps_init_all(void);

/**
 * @brief GPS 태스크 생성 (레거시 호환용)
 *
 * @param arg 사용하지 않음 (board_config로 자동 설정)
 */
void gps_task_create(void *arg);

/**
 * @brief GPS 핸들 가져오기 (레거시 호환성)
 *
 * @return GPS 핸들 포인터 (첫 번째 GPS)
 */
gps_t* gps_get_handle(void);

/**
 * @brief GPS 인스턴스 핸들 가져오기
 *
 * @param id GPS ID
 * @return GPS 핸들 포인터
 */
gps_t* gps_get_instance_handle(gps_id_t id);

/**
 * @brief GGA 평균 데이터 읽기 가능 여부
 *
 * @param id GPS ID
 * @return true: 읽기 가능, false: 읽기 불가능
 */
bool gps_gga_avg_can_read(gps_id_t id);

/**
 * @brief GGA 평균 데이터 가져오기
 *
 * @param id GPS ID
 * @param lat 위도 출력 (NULL 가능)
 * @param lon 경도 출력 (NULL 가능)
 * @param alt 고도 출력 (NULL 가능)
 * @return true: 성공, false: 실패
 */
bool gps_get_gga_avg(gps_id_t id, double* lat, double* lon, double* alt);

/**
 * @brief GPS 명령 전송 (동기식, 응답 대기)
 *
 * @param id GPS ID
 * @param cmd 전송할 명령 문자열
 * @param response 응답 버퍼 (NULL 가능)
 * @param resp_len 응답 버퍼 크기
 * @param timeout_ms 타임아웃 (밀리초)
 * @return true: 성공 (OK 응답), false: 실패 (ERROR 또는 타임아웃)
 */
bool gps_send_command_sync(gps_id_t id, const char* cmd, char* response,
                           size_t resp_len, uint32_t timeout_ms);

#endif
