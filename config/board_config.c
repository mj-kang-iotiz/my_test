#include "board_config.h"
#include "board_pinmap.h"
#include "gpio.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

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

/* ============================================================
 * 내부 함수 선언
 * ============================================================ */

static void board_init_f9p_gps(uint8_t instance);
static void board_init_um982_gps(uint8_t instance);
static bool um982_wait_for_ready(uint32_t timeout_ms);

/* ============================================================
 * 공통 함수
 * ============================================================ */

const board_config_t* board_get_config(void) {
    return &current_config;
}

/* ============================================================
 * 보드 초기화 메인 함수
 * ============================================================ */

void board_init(void) {
    // 1. 공통 모듈 초기화 (LED, LTE 등)
    // LED는 별도 초기화 함수가 있다고 가정
    // led_init();

    // 2. GPS 초기화
#if GPS_COUNT >= 1
    board_init_gps(GPS_PRIMARY, 0);
#endif
#if GPS_COUNT >= 2
    board_init_gps(GPS_SECONDARY, 1);
#endif

    // 3. 통신 인터페이스 초기화
    board_init_comm_interfaces();
}

/* ============================================================
 * GPS 초기화
 * ============================================================ */

void board_init_gps(gps_type_t gps_type, uint8_t instance) {
    switch(gps_type) {
        case GPS_TYPE_F9P:
            board_init_f9p_gps(instance);
            break;

        case GPS_TYPE_UM982:
            board_init_um982_gps(instance);
            break;

        case GPS_TYPE_NONE:
        default:
            // GPS 없음
            break;
    }
}

/**
 * @brief F9P GPS 초기화
 * F9P는 전원 인가 후 즉시 동작 가능
 */
static void board_init_f9p_gps(uint8_t instance) {
    // 1. Reset 핀 제어
    HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_SET);

    // 2. UART 초기화는 이미 MX_USART2_UART_Init()에서 되어있다고 가정
    // 또는 여기서 직접 초기화

    // 3. F9P는 즉시 사용 가능 - 별도 대기 불필요

    // 4. 필요시 UBX 프로토콜 설정 명령 전송
    // 예: 출력 메시지 타입 설정, 업데이트 레이트 등

    // TODO: F9P 전용 설정 함수 호출
    // gps_f9p_configure(instance);
}

/**
 * @brief UM982 GPS 초기화
 * UM982는 RDY 신호를 확인해야 함
 */
static void board_init_um982_gps(uint8_t instance) {
    // 1. Reset 핀 제어
    HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(100));
    HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_SET);

    // 2. RDY 신호 대기 (최대 5초)
    if (!um982_wait_for_ready(5000)) {
        // RDY 신호 수신 실패 - 에러 처리
        // LOG_ERR("UM982 RDY timeout!");
        return;
    }

    // 3. UART 초기화는 이미 되어있다고 가정

    // 4. UM982 설정 명령 전송
    // TODO: UM982 전용 설정 함수 호출
    // gps_um982_configure(instance);
}

/**
 * @brief UM982 RDY 신호 대기
 * @param timeout_ms 타임아웃 (밀리초)
 * @return true: RDY 수신, false: 타임아웃
 */
static bool um982_wait_for_ready(uint32_t timeout_ms) {
#if defined(BOARD_TYPE_PCB2) || defined(BOARD_TYPE_PCB4)
    // UM982 사용 보드만 RDY 핀 확인
    uint32_t start_tick = xTaskGetTickCount();
    uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
        if (HAL_GPIO_ReadPin(GPS1_RDY_PORT, GPS1_RDY_PIN) == GPIO_PIN_SET) {
            return true;  // RDY 신호 확인!
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 대기
    }

    return false;  // 타임아웃
#else
    // F9P 보드는 RDY 핀이 없으므로 즉시 true 반환
    return true;
#endif
}

/* ============================================================
 * 통신 인터페이스 초기화
 * ============================================================ */

void board_init_comm_interfaces(void) {
    const board_config_t* config = board_get_config();

    // 1. BLE 초기화 (PCB1, PCB2만)
#if HAS_BLE
    if (config->comm_interfaces & COMM_TYPE_BLE) {
        // BLE 모듈 리셋
        HAL_GPIO_WritePin(BLE_RESET_PORT, BLE_RESET_PIN, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(100));
        HAL_GPIO_WritePin(BLE_RESET_PORT, BLE_RESET_PIN, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(500));  // BLE 모듈 부팅 대기

        // TODO: BLE 초기화 함수 호출
        // ble_init();
    }
#endif

    // 2. RS485 초기화 (PCB3, PCB4만)
#if HAS_RS485
    if (config->comm_interfaces & COMM_TYPE_RS485) {
        // RS485 DE/RE 핀 초기화 (수신 모드)
        HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);  // DE = 0
        HAL_GPIO_WritePin(RS485_RE_PORT, RS485_RE_PIN, GPIO_PIN_RESET);  // RE = 0 (수신 활성)

        // TODO: RS485 초기화 함수 호출
        // rs485_init();
    }
#endif

    // 3. LoRa 초기화 (모든 보드)
    if (config->comm_interfaces & COMM_TYPE_LORA) {
        // LoRa 모듈 리셋
        HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(100));
        HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(500));  // LoRa 모듈 부팅 대기

        // TODO: LoRa 초기화 함수 호출
        // lora_init();
    }

    // 4. LTE(EC25) 초기화 (모든 보드)
    // TODO: LTE 초기화 함수 호출
    // lte_init();
}

/* ============================================================
 * 유틸리티 함수
 * ============================================================ */

/**
 * @brief 현재 보드의 GPS 개수 반환
 */
uint8_t board_get_gps_count(void) {
    return current_config.gps_count;
}

/**
 * @brief 특정 통신 인터페이스 사용 여부 확인
 */
bool board_has_interface(comm_type_t comm_type) {
    return (current_config.comm_interfaces & comm_type) != 0;
}
