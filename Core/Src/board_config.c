#include "board_config.h"
#include "gpio.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * 컴파일 타임 보드 설정
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

void board_init(void) {
    // GPS 초기화
#if GPS_COUNT >= 1
    board_init_gps(GPS_PRIMARY, 0);
#endif
#if GPS_COUNT >= 2
    board_init_gps(GPS_SECONDARY, 1);
#endif

    // 통신 인터페이스 초기화
    board_init_comm_interfaces();
}

/* ============================================================
 * 공통 함수
 * ============================================================ */

const board_config_t* board_get_config(void) {
    return &current_config;
}

void board_init_gps(gps_type_t gps_type, uint8_t instance) {
    switch(gps_type) {
        case GPS_TYPE_F9P:
            // F9P GPS 초기화
            // - UART 설정 (보통 38400 또는 115200 baud)
            // - UBX 프로토콜 설정
            // TODO: F9P 전용 초기화 함수 호출
            break;

        case GPS_TYPE_UM982:
            // UM982 GPS 초기화
            // - UART 설정
            // - NMEA + 바이너리 프로토콜 설정
            // TODO: UM982 전용 초기화 함수 호출
            break;

        case GPS_TYPE_NONE:
        default:
            // GPS 없음
            break;
    }
}

void board_init_comm_interfaces(void) {
    const board_config_t* config = board_get_config();

    // BLE 초기화
    if (config->comm_interfaces & COMM_TYPE_BLE) {
        // TODO: BLE 모듈 초기화
        // - UART 설정
        // - AT 명령어로 BLE 설정
    }

    // RS485 초기화
    if (config->comm_interfaces & COMM_TYPE_RS485) {
        // TODO: RS485 드라이버 초기화
        // - UART 설정
        // - DE/RE 핀 설정
    }

    // LoRa 초기화
    if (config->comm_interfaces & COMM_TYPE_LORA) {
        // TODO: LoRa 모듈 초기화
        // - SPI 또는 UART 설정
        // - LoRa 파라미터 설정
    }
}
