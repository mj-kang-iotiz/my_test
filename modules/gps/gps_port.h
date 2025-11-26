#ifndef GPS_PORT_H
#define GPS_PORT_H

#include "gps.h"
#include "board_config.h"
#include "FreeRTOS.h"
#include "queue.h"

/**
 * @brief GPS 인스턴스 초기화 (보드 설정 기반)
 *
 * @param gps_handle GPS 핸들 포인터
 * @param id GPS ID (GPS_ID_BASE 또는 GPS_ID_ROVER)
 * @param type GPS 타입 (GPS_TYPE_F9P 또는 GPS_TYPE_UM982)
 * @return 0: 성공, -1: 실패
 */
int gps_port_init_instance(gps_t* gps_handle, gps_id_t id, gps_type_t type);

/**
 * @brief GPS 통신 시작
 *
 * @param gps_handle GPS 핸들 포인터
 */
void gps_port_start(gps_t* gps_handle);

/**
 * @brief GPS 통신 정지
 *
 * @param gps_handle GPS 핸들 포인터
 */
void gps_port_stop(gps_t* gps_handle);

/**
 * @brief GPS 데이터 수신 위치 가져오기
 *
 * @param id GPS ID
 * @return 수신 버퍼 현재 위치
 */
uint32_t gps_port_get_rx_pos(gps_id_t id);

/**
 * @brief GPS 수신 버퍼 포인터 가져오기
 *
 * @param id GPS ID
 * @return 수신 버퍼 포인터
 */
char* gps_port_get_recv_buf(gps_id_t id);

/**
 * @brief GPS 인터럽트 핸들러용 큐 설정
 *
 * @param id GPS ID
 * @param queue 큐 핸들
 */
void gps_port_set_queue(gps_id_t id, QueueHandle_t queue);

#endif // GPS_PORT_H
