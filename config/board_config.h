#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "board_type.h"

/**
 * ============================================================================
 * 보드별 설정
 * ============================================================================
 */
#if defined(BOARD_TYPE_BASE_UNICORE)
    #define BOARD_TYPE BOARD_TYPE_BASE_UM982
    #define GPS1_TYPE GPS_TYPE_UM982
    #define GPS2_TYPE GPS_TYPE_NONE
    #define GPS_CNT 1
    #define LORA_MODE LORA_MODE_TX_ONLY
    #define USE_BLE 1
    #define USE_RS485 0
    #define USE_GSM 1

#elif defined(BOARD_TYPE_BASE_UBLOX)
    #define BOARD_TYPE BOARD_TYPE_BASE_F9P
    #define GPS1_TYPE GPS_TYPE_F9P
    #define GPS2_TYPE GPS_TYPE_NONE
    #define GPS_CNT 1
    #define LORA_MODE LORA_MODE_TX_ONLY
    #define USE_BLE 1
    #define USE_RS485 0
    #define USE_GSM 1

#elif defined(BOARD_TYPE_ROVER_UNICORE)
    #define BOARD_TYPE BOARD_TYPE_ROVER_UM982
    #define GPS1_TYPE GPS_TYPE_UM982
    #define GPS2_TYPE GPS_TYPE_NONE
    #define GPS_CNT 1
    #define LORA_MODE LORA_MODE_RX_ONLY
    #define USE_BLE 0
    #define USE_RS485 1
    #define USE_GSM 1

#elif defined(BOARD_TYPE_ROVER_UBLOX)
    #define BOARD_TYPE BOARD_TYPE_ROVER_F9P
    #define GPS1_TYPE GPS_TYPE_F9P
    #define GPS2_TYPE GPS_TYPE_F9P
    #define GPS_CNT 2  // ✅ F9P 2개
    #define LORA_MODE LORA_MODE_RX_ONLY
    #define USE_BLE 0
    #define USE_RS485 1
    #define USE_GSM 1

#else
    #define BOARD_TYPE BOARD_TYPE_NONE
    #define GPS1_TYPE GPS_TYPE_NONE
    #define GPS2_TYPE GPS_TYPE_NONE
    #define GPS_CNT 0
    #define LORA_MODE LORA_MODE_NONE
    #define USE_BLE 0
    #define USE_RS485 0
    #define USE_GSM 0
#endif

/**
 * ============================================================================
 * 타입 정의
 * ============================================================================
 */
typedef enum
{
    BOARD_TYPE_NONE = 0,
    BOARD_TYPE_BASE_UM982,
    BOARD_TYPE_BASE_F9P,
    BOARD_TYPE_ROVER_UM982,
    BOARD_TYPE_ROVER_F9P
} board_type_t;

typedef enum
{
    GPS_TYPE_NONE = 0,
    GPS_TYPE_F9P,
    GPS_TYPE_UM982,
} gps_type_t;

typedef enum {
    GPS_ID_BASE = 0,    // 기준국 GPS 또는 로버 첫 번째
    GPS_ID_ROVER,       // 로버 두 번째 GPS (ROVER_UBLOX만 해당)
    GPS_ID_MAX
} gps_id_t;

typedef enum
{
    LORA_MODE_NONE = 0,
    LORA_MODE_TX_ONLY,
    LORA_MODE_RX_ONLY
} lora_mode_t;

/**
 * @brief 보드 설정 구조체
 */
typedef struct
{
    board_type_t board;
    gps_type_t gps[GPS_ID_MAX];
    uint8_t gps_cnt;
    lora_mode_t lora_mode;
    bool use_ble;
    bool use_rs485;
    bool use_gsm;
} board_config_t;

/**
 * @brief 현재 보드 설정 가져오기
 */
const board_config_t* board_get_config(void);

#endif // BOARD_CONFIG_H
