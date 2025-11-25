#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// 보드 타입 설정 파일 포함
#include "board_type.h"

/**
 * @brief 보드 타입 정의
 */
typedef enum {
    BOARD_TYPE_UNKNOWN = 0,
    BOARD_TYPE_PCB1_F9P_BLE,      // F9P + BLE + LoRa
    BOARD_TYPE_PCB2_UM982_BLE,    // UM982 + BLE + LoRa
    BOARD_TYPE_PCB3_F9P_RS485,    // F9P x2 + RS485 + LoRa
    BOARD_TYPE_PCB4_UM982_RS485,  // UM982 + RS485 + LoRa
    BOARD_TYPE_MAX
} board_type_t;

/**
 * @brief GPS 타입
 */
typedef enum {
    GPS_TYPE_NONE = 0,
    GPS_TYPE_F9P,
    GPS_TYPE_UM982,
} gps_type_t;

/**
 * @brief 통신 타입 (비트마스크)
 */
typedef enum {
    COMM_TYPE_NONE = 0,
    COMM_TYPE_BLE = (1 << 0),
    COMM_TYPE_RS485 = (1 << 1),
    COMM_TYPE_LORA = (1 << 2),
} comm_type_t;

/**
 * @brief 보드 설정 구조체
 */
typedef struct {
    board_type_t board_type;
    gps_type_t gps_primary;       // 메인 GPS
    gps_type_t gps_secondary;     // 서브 GPS (듀얼 GPS 보드용)
    uint8_t gps_count;            // GPS 개수
    uint8_t comm_interfaces;      // 통신 인터페이스 (비트마스크)
    const char* board_name;
} board_config_t;

/* ============================================================
 * 컴파일 타임 보드 설정
 * ============================================================ */

#if defined(BOARD_TYPE_PCB1)
    #define CURRENT_BOARD BOARD_TYPE_PCB1_F9P_BLE
    #define GPS_PRIMARY GPS_TYPE_F9P
    #define GPS_SECONDARY GPS_TYPE_NONE
    #define GPS_COUNT 1
    #define HAS_BLE 1
    #define HAS_RS485 0
    #define HAS_LORA 1

#elif defined(BOARD_TYPE_PCB2)
    #define CURRENT_BOARD BOARD_TYPE_PCB2_UM982_BLE
    #define GPS_PRIMARY GPS_TYPE_UM982
    #define GPS_SECONDARY GPS_TYPE_NONE
    #define GPS_COUNT 1
    #define HAS_BLE 1
    #define HAS_RS485 0
    #define HAS_LORA 1

#elif defined(BOARD_TYPE_PCB3)
    #define CURRENT_BOARD BOARD_TYPE_PCB3_F9P_RS485
    #define GPS_PRIMARY GPS_TYPE_F9P
    #define GPS_SECONDARY GPS_TYPE_F9P
    #define GPS_COUNT 2
    #define HAS_BLE 0
    #define HAS_RS485 1
    #define HAS_LORA 1

#elif defined(BOARD_TYPE_PCB4)
    #define CURRENT_BOARD BOARD_TYPE_PCB4_UM982_RS485
    #define GPS_PRIMARY GPS_TYPE_UM982
    #define GPS_SECONDARY GPS_TYPE_NONE
    #define GPS_COUNT 1
    #define HAS_BLE 0
    #define HAS_RS485 1
    #define HAS_LORA 1

#else
    #error "Board type not defined! Define one of: BOARD_TYPE_PCB1, BOARD_TYPE_PCB2, BOARD_TYPE_PCB3, BOARD_TYPE_PCB4"
#endif

/* ============================================================
 * API 함수
 * ============================================================ */

/**
 * @brief 현재 보드 설정 가져오기
 * @return const board_config_t* 보드 설정 포인터
 */
const board_config_t* board_get_config(void);

/**
 * @brief 현재 보드의 GPS 개수 반환
 * @return GPS 개수
 */
uint8_t board_get_gps_count(void);

/**
 * @brief 특정 통신 인터페이스 사용 여부 확인
 * @param comm_type 확인할 통신 타입
 * @return true: 사용, false: 사용 안 함
 */
bool board_has_interface(comm_type_t comm_type);

#endif /* BOARD_CONFIG_H */
