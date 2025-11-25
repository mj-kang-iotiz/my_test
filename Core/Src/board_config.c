#include "board_config.h"
#include "gpio.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * 방법 1: 런타임 감지 방식
 * ============================================================ */
#ifdef BOARD_RUNTIME_DETECT

// 보드 ID 감지용 GPIO 핀 정의 (예시 - 실제 핀은 하드웨어에 맞게 수정)
#define BOARD_ID_PIN0_GPIO_Port GPIOB
#define BOARD_ID_PIN0_Pin       GPIO_PIN_12
#define BOARD_ID_PIN1_GPIO_Port GPIOB
#define BOARD_ID_PIN1_Pin       GPIO_PIN_13

static board_config_t current_config;

/**
 * @brief 보드 타입 자동 감지
 *
 * GPIO 핀 2개의 풀업/풀다운 상태로 4가지 보드 구분
 * - 핀을 플로팅으로 두고 보드마다 다르게 풀업/풀다운 저항 연결
 * - 또는 ADC로 전압 분배 저항으로 구분
 */
board_type_t board_detect_type(void) {
    // GPIO 핀 읽기
    uint8_t id0 = HAL_GPIO_ReadPin(BOARD_ID_PIN0_GPIO_Port, BOARD_ID_PIN0_Pin);
    uint8_t id1 = HAL_GPIO_ReadPin(BOARD_ID_PIN1_GPIO_Port, BOARD_ID_PIN1_Pin);

    uint8_t board_id = (id1 << 1) | id0;

    switch(board_id) {
        case 0: // 00
            return BOARD_TYPE_PCB1_F9P_BLE;
        case 1: // 01
            return BOARD_TYPE_PCB2_UM982_BLE;
        case 2: // 10
            return BOARD_TYPE_PCB3_F9P_RS485;
        case 3: // 11
            return BOARD_TYPE_PCB4_UM982_RS485;
        default:
            return BOARD_TYPE_UNKNOWN;
    }
}

/**
 * @brief 보드 타입에 따른 설정 초기화
 */
static void board_setup_config(board_type_t type) {
    current_config.board_type = type;

    switch(type) {
        case BOARD_TYPE_PCB1_F9P_BLE:
            current_config.gps_primary = GPS_TYPE_F9P;
            current_config.gps_secondary = GPS_TYPE_NONE;
            current_config.gps_count = 1;
            current_config.comm_interfaces = COMM_TYPE_BLE | COMM_TYPE_LORA;
            current_config.board_name = "PCB1 (F9P+BLE)";
            break;

        case BOARD_TYPE_PCB2_UM982_BLE:
            current_config.gps_primary = GPS_TYPE_UM982;
            current_config.gps_secondary = GPS_TYPE_NONE;
            current_config.gps_count = 1;
            current_config.comm_interfaces = COMM_TYPE_BLE | COMM_TYPE_LORA;
            current_config.board_name = "PCB2 (UM982+BLE)";
            break;

        case BOARD_TYPE_PCB3_F9P_RS485:
            current_config.gps_primary = GPS_TYPE_F9P;
            current_config.gps_secondary = GPS_TYPE_F9P;  // 듀얼 GPS
            current_config.gps_count = 2;
            current_config.comm_interfaces = COMM_TYPE_RS485 | COMM_TYPE_LORA;
            current_config.board_name = "PCB3 (F9P x2+RS485)";
            break;

        case BOARD_TYPE_PCB4_UM982_RS485:
            current_config.gps_primary = GPS_TYPE_UM982;
            current_config.gps_secondary = GPS_TYPE_NONE;
            current_config.gps_count = 1;
            current_config.comm_interfaces = COMM_TYPE_RS485 | COMM_TYPE_LORA;
            current_config.board_name = "PCB4 (UM982+RS485)";
            break;

        default:
            current_config.gps_primary = GPS_TYPE_NONE;
            current_config.gps_secondary = GPS_TYPE_NONE;
            current_config.gps_count = 0;
            current_config.comm_interfaces = COMM_TYPE_NONE;
            current_config.board_name = "Unknown Board";
            break;
    }
}

void board_init(void) {
    // 보드 타입 자동 감지
    board_type_t detected_type = board_detect_type();

    // 설정 초기화
    board_setup_config(detected_type);

    // GPS 초기화
    if (current_config.gps_count >= 1) {
        board_init_gps(current_config.gps_primary, 0);
    }
    if (current_config.gps_count >= 2) {
        board_init_gps(current_config.gps_secondary, 1);
    }

    // 통신 인터페이스 초기화
    board_init_comm_interfaces();
}

#else
/* ============================================================
 * 방법 2: 컴파일 타임 방식
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

#endif /* BOARD_RUNTIME_DETECT */

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
