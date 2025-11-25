#include "board_config.h"

/* ============================================================
 * 보드 설정 (컴파일 타임)
 * ============================================================ */

static const board_config_t current_config = {
    .board_type = CURRENT_BOARD,
    .gps_primary = GPS_PRIMARY,
    .gps_secondary = GPS_SECONDARY,
    .gps_count = GPS_COUNT,
    .comm_interfaces =
        (HAS_BLE ? COMM_TYPE_BLE : 0) |
        (HAS_RS485 ? COMM_TYPE_RS485 : 0) |
        (HAS_LORA ? COMM_TYPE_LORA : 0),
#if defined(BOARD_TYPE_PCB1)
    .board_name = "PCB1 (F9P+BLE)"
#elif defined(BOARD_TYPE_PCB2)
    .board_name = "PCB2 (UM982+BLE)"
#elif defined(BOARD_TYPE_PCB3)
    .board_name = "PCB3 (F9P x2+RS485)"
#elif defined(BOARD_TYPE_PCB4)
    .board_name = "PCB4 (UM982+RS485)"
#endif
};

/* ============================================================
 * API 함수
 * ============================================================ */

const board_config_t* board_get_config(void) {
    return &current_config;
}

uint8_t board_get_gps_count(void) {
    return current_config.gps_count;
}

bool board_has_interface(comm_type_t comm_type) {
    return (current_config.comm_interfaces & comm_type) != 0;
}
