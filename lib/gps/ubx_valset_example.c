/**
 * @file ubx_valset_example.c
 * @brief UBX VAL-SET/GET 사용 예시
 *
 * 이 파일은 u-blox GPS 모듈에서 VAL-SET/GET 명령을 사용하는 방법을 보여줍니다.
 */

#include "gps.h"
#include "gps_ubx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// UBX Configuration Key IDs (예시)
#define CFG_RATE_MEAS       0x30210001  // Measurement rate (U2, ms)
#define CFG_RATE_NAV        0x30210002  // Navigation rate (U2, cycles)
#define CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1  0x209100a5  // NAV-HPPOSLLH output rate (U1)

/**
 * 예시 1: 동기 방식 (가장 간단)
 * - 함수 내부에서 ACK/NAK을 기다림
 * - 타임아웃 지원
 */
void example_sync(gps_t *gps) {
    printf("=== Example 1: 동기 방식 ===\n");

    // 1개 설정 변경
    ubx_cfg_item_t items[1];

    // Measurement rate를 100ms로 설정
    uint16_t rate = 100;
    items[0].key_id = CFG_RATE_MEAS;
    memcpy(items[0].value, &rate, sizeof(rate));
    items[0].value_len = sizeof(rate);

    // 동기 전송 (3초 타임아웃)
    bool result = ubx_send_valset_sync(gps, UBX_LAYER_RAM, items, 1, 3000);

    if (result) {
        printf("✓ Configuration updated successfully\n");
    } else {
        printf("✗ Configuration failed (NAK or timeout)\n");
    }
}

/**
 * 예시 2: 여러 설정을 한 번에 변경 (동기)
 */
void example_multiple_configs(gps_t *gps) {
    printf("=== Example 2: 여러 설정 변경 ===\n");

    ubx_cfg_item_t items[3];

    // 1. Measurement rate: 200ms
    uint16_t meas_rate = 200;
    items[0].key_id = CFG_RATE_MEAS;
    memcpy(items[0].value, &meas_rate, sizeof(meas_rate));
    items[0].value_len = sizeof(meas_rate);

    // 2. Navigation rate: 1 cycle
    uint16_t nav_rate = 1;
    items[1].key_id = CFG_RATE_NAV;
    memcpy(items[1].value, &nav_rate, sizeof(nav_rate));
    items[1].value_len = sizeof(nav_rate);

    // 3. NAV-HPPOSLLH output rate: 1 (매번 출력)
    uint8_t output_rate = 1;
    items[2].key_id = CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1;
    items[2].value[0] = output_rate;
    items[2].value_len = 1;

    // 한 번에 전송
    if (ubx_send_valset_sync(gps, UBX_LAYER_RAM, items, 3, 5000)) {
        printf("✓ All configurations updated\n");
    } else {
        printf("✗ Configuration failed\n");
    }
}

/**
 * 예시 3: 비동기 방식 (폴링)
 * - 전송 후 바로 리턴
 * - 나중에 상태 확인
 */
void example_async_polling(gps_t *gps) {
    printf("=== Example 3: 비동기 방식 (폴링) ===\n");

    ubx_cfg_item_t items[1];
    uint16_t rate = 100;
    items[0].key_id = CFG_RATE_MEAS;
    memcpy(items[0].value, &rate, sizeof(rate));
    items[0].value_len = sizeof(rate);

    // 비동기 전송 (바로 리턴)
    if (!ubx_send_valset(gps, UBX_LAYER_RAM, items, 1)) {
        printf("✗ Failed to send (another command is pending)\n");
        return;
    }

    printf("Command sent, waiting for response...\n");

    // 다른 작업 수행 가능...
    vTaskDelay(pdMS_TO_TICKS(50));

    // 상태 확인 (폴링)
    uint32_t timeout_ms = 3000;
    while (1) {
        ubx_cmd_state_t state = ubx_get_cmd_state(&gps->ubx_cmd_handler, timeout_ms);

        if (state == UBX_CMD_STATE_ACK) {
            printf("✓ ACK received\n");
            break;
        } else if (state == UBX_CMD_STATE_NAK) {
            printf("✗ NAK received\n");
            break;
        } else if (state == UBX_CMD_STATE_TIMEOUT) {
            printf("✗ Timeout\n");
            break;
        } else if (state == UBX_CMD_STATE_WAITING) {
            printf("  Still waiting...\n");
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/**
 * 예시 4: 콜백 방식
 * - ACK/NAK 받으면 자동으로 콜백 호출
 */
static void ack_callback(bool ack, void *user_data) {
    const char *config_name = (const char *)user_data;

    if (ack) {
        printf("✓ [%s] ACK received (callback)\n", config_name);
    } else {
        printf("✗ [%s] NAK received (callback)\n", config_name);
    }
}

void example_callback(gps_t *gps) {
    printf("=== Example 4: 콜백 방식 ===\n");

    ubx_cfg_item_t items[1];
    uint16_t rate = 100;
    items[0].key_id = CFG_RATE_MEAS;
    memcpy(items[0].value, &rate, sizeof(rate));
    items[0].value_len = sizeof(rate);

    // 콜백과 함께 전송
    if (!ubx_send_valset_cb(gps, UBX_LAYER_RAM, items, 1,
                            ack_callback, (void*)"Measurement Rate")) {
        printf("✗ Failed to send\n");
        return;
    }

    printf("Command sent with callback\n");

    // 콜백은 파서에서 ACK/NAK 수신 시 자동 호출됨
    // 여기서는 다른 작업 수행 가능
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * 예시 5: Flash에 저장 (재부팅 후에도 유지)
 */
void example_save_to_flash(gps_t *gps) {
    printf("=== Example 5: Flash에 저장 ===\n");

    ubx_cfg_item_t items[1];
    uint16_t rate = 200;
    items[0].key_id = CFG_RATE_MEAS;
    memcpy(items[0].value, &rate, sizeof(rate));
    items[0].value_len = sizeof(rate);

    // Flash layer에 저장 (재부팅 후에도 유지)
    if (ubx_send_valset_sync(gps, UBX_LAYER_FLASH, items, 1, 5000)) {
        printf("✓ Configuration saved to Flash\n");
    } else {
        printf("✗ Failed to save to Flash\n");
    }
}

/**
 * 예시 6: VAL-GET (설정값 읽기)
 */
void example_valget(gps_t *gps) {
    printf("=== Example 6: VAL-GET ===\n");

    uint32_t key_ids[] = {
        CFG_RATE_MEAS,
        CFG_RATE_NAV,
    };

    // VAL-GET 전송
    if (ubx_send_valget(gps, UBX_LAYER_RAM, key_ids, 2)) {
        printf("VAL-GET request sent\n");
        // 응답은 UBX-CFG-VALGET 메시지로 수신됨
        // 파싱은 별도 구현 필요
    } else {
        printf("✗ Failed to send VAL-GET\n");
    }
}

/**
 * 예시 7: 초기화 시퀀스 (권장)
 */
bool ublox_initialize_device(gps_t *gps) {
    printf("=== Example 7: 초기화 시퀀스 ===\n");

    // 초기화 설정 목록
    typedef struct {
        const char *name;
        ubx_cfg_item_t item;
    } init_config_t;

    init_config_t configs[] = {
        {
            .name = "Measurement rate: 100ms",
            .item = {
                .key_id = CFG_RATE_MEAS,
                .value = {100, 0},  // 100ms (little-endian)
                .value_len = 2,
            }
        },
        {
            .name = "Navigation rate: 1 cycle",
            .item = {
                .key_id = CFG_RATE_NAV,
                .value = {1, 0},
                .value_len = 2,
            }
        },
        {
            .name = "Enable NAV-HPPOSLLH output",
            .item = {
                .key_id = CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1,
                .value = {1},  // Output every cycle
                .value_len = 1,
            }
        },
    };

    // 각 설정을 순차적으로 전송
    for (size_t i = 0; i < sizeof(configs) / sizeof(configs[0]); i++) {
        printf("  [%zu/%zu] %s...\n", i + 1,
               sizeof(configs) / sizeof(configs[0]),
               configs[i].name);

        if (!ubx_send_valset_sync(gps, UBX_LAYER_RAM, &configs[i].item, 1, 3000)) {
            printf("  ✗ Failed at step %zu\n", i + 1);
            return false;
        }

        printf("  ✓ OK\n");
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms 대기
    }

    printf("✓ Initialization complete\n");
    return true;
}

/**
 * 메인 예시 태스크
 */
void ubx_example_task(void *pvParameters) {
    gps_t *gps = (gps_t *)pvParameters;

    // GPS 초기화 대기
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 예시 실행
    example_sync(gps);
    vTaskDelay(pdMS_TO_TICKS(500));

    example_multiple_configs(gps);
    vTaskDelay(pdMS_TO_TICKS(500));

    example_async_polling(gps);
    vTaskDelay(pdMS_TO_TICKS(500));

    example_callback(gps);
    vTaskDelay(pdMS_TO_TICKS(3500));

    example_save_to_flash(gps);
    vTaskDelay(pdMS_TO_TICKS(500));

    example_valget(gps);
    vTaskDelay(pdMS_TO_TICKS(500));

    ublox_initialize_device(gps);

    printf("\n=== All examples completed ===\n");

    vTaskDelete(NULL);
}
