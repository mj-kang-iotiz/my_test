/**
 * @file ubx_async_init_example.c
 * @brief UBX 비동기 초기화 사용 예시
 *
 * 외부 태스크에서 블로킹 없이 초기화를 진행하는 방법을 보여줍니다.
 */

#include "gps.h"
#include "gps_ubx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// Configuration Key IDs
#define CFG_RATE_MEAS       0x30210001
#define CFG_RATE_NAV        0x30210002
#define CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1  0x209100a5

/**
 * 예시 1: 기본 비동기 초기화
 */

// 전역 또는 static 배열로 설정 유지 (스택이 아님!)
static const ubx_cfg_item_t g_ublox_configs[] = {
    {
        .key_id = CFG_RATE_MEAS,
        .value = {100, 0},  // 100ms
        .value_len = 2,
    },
    {
        .key_id = CFG_RATE_NAV,
        .value = {1, 0},    // 1 cycle
        .value_len = 2,
    },
    {
        .key_id = CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1,
        .value = {1},       // Enable
        .value_len = 1,
    },
};

// 초기화 완료 콜백
static void on_init_complete(bool success, size_t failed_step, void *user_data) {
    if (success) {
        printf("✓ UBX initialization completed successfully!\n");
    } else {
        printf("✗ UBX initialization failed at step %zu\n", failed_step);
    }
}

void example1_basic_async_init(gps_t *gps) {
    printf("=== Example 1: 기본 비동기 초기화 ===\n");

    // 비동기 초기화 시작
    bool started = ubx_init_async_start(
        gps,
        UBX_LAYER_RAM,
        g_ublox_configs,
        sizeof(g_ublox_configs) / sizeof(g_ublox_configs[0]),
        on_init_complete,
        NULL
    );

    if (!started) {
        printf("✗ Failed to start async init (already running?)\n");
        return;
    }

    printf("Async init started, continuing with other tasks...\n");

    // 이제 다른 작업 수행 가능!
    // ubx_init_async_process()는 주기적으로 호출되어야 함
}

/**
 * 예시 2: 메인 루프에서 process() 호출
 */
void main_task(void *pvParameters) {
    gps_t *gps = (gps_t *)pvParameters;

    // GPS 초기화
    gps_init(gps);

    // 비동기 초기화 시작
    ubx_init_async_start(
        gps,
        UBX_LAYER_RAM,
        g_ublox_configs,
        sizeof(g_ublox_configs) / sizeof(g_ublox_configs[0]),
        on_init_complete,
        NULL
    );

    // 메인 루프
    while (1) {
        // 비동기 초기화 진행 (블로킹 없음!)
        ubx_init_async_process(gps);

        // 다른 작업들...
        do_other_work();

        // 10ms 대기
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * 예시 3: 타이머에서 process() 호출
 */
static TimerHandle_t g_ubx_timer;

static void ubx_timer_callback(TimerHandle_t xTimer) {
    gps_t *gps = (gps_t *)pvTimerGetTimerID(xTimer);

    // 비동기 초기화 진행
    ubx_init_async_process(gps);

    // 완료 확인
    if (ubx_init_async_get_state(gps) == UBX_INIT_STATE_DONE) {
        // 타이머 정지
        xTimerStop(xTimer, 0);
        printf("Init done, timer stopped\n");
    }
}

void example3_timer_based(gps_t *gps) {
    printf("=== Example 3: 타이머 기반 초기화 ===\n");

    // 비동기 초기화 시작
    ubx_init_async_start(
        gps,
        UBX_LAYER_RAM,
        g_ublox_configs,
        sizeof(g_ublox_configs) / sizeof(g_ublox_configs[0]),
        on_init_complete,
        NULL
    );

    // 타이머 생성 (100ms마다 호출)
    g_ubx_timer = xTimerCreate(
        "UBX_Timer",
        pdMS_TO_TICKS(100),
        pdTRUE,  // Auto-reload
        gps,     // Timer ID (gps 포인터)
        ubx_timer_callback
    );

    xTimerStart(g_ubx_timer, 0);

    printf("Timer started, init will proceed in background\n");
}

/**
 * 예시 4: 상태 확인 및 취소
 */
void example4_state_check(gps_t *gps) {
    printf("=== Example 4: 상태 확인 및 취소 ===\n");

    // 초기화 시작
    ubx_init_async_start(
        gps,
        UBX_LAYER_RAM,
        g_ublox_configs,
        sizeof(g_ublox_configs) / sizeof(g_ublox_configs[0]),
        on_init_complete,
        NULL
    );

    // 메인 루프
    for (int i = 0; i < 100; i++) {
        ubx_init_async_process(gps);

        // 상태 확인
        ubx_init_state_t state = ubx_init_async_get_state(gps);

        if (state == UBX_INIT_STATE_DONE) {
            printf("✓ Init completed at iteration %d\n", i);
            break;
        } else if (state == UBX_INIT_STATE_ERROR) {
            printf("✗ Init failed at iteration %d\n", i);
            break;
        } else if (state == UBX_INIT_STATE_RUNNING) {
            printf("  [%d] Init in progress...\n", i);
        }

        // 특정 조건에서 취소
        if (i == 50 && state == UBX_INIT_STATE_RUNNING) {
            printf("⚠ Cancelling init at iteration 50\n");
            ubx_init_async_cancel(gps);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * 예시 5: 여러 단계 초기화 (더 많은 설정)
 */
static const ubx_cfg_item_t g_full_configs[] = {
    {CFG_RATE_MEAS, {100, 0}, 2},
    {CFG_RATE_NAV, {1, 0}, 2},
    {CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1, {1}, 1},
    // 더 많은 설정...
    {0x10520001, {115200, 0x00, 0x01, 0x00}, 4},  // UART1 baudrate
    {0x10710001, {5, 0}, 2},                      // Min SV elevation
};

static void on_full_init_complete(bool success, size_t failed_step, void *user_data) {
    if (success) {
        printf("✓ Full initialization completed (%zu steps)\n",
               sizeof(g_full_configs) / sizeof(g_full_configs[0]));
    } else {
        printf("✗ Full initialization failed at step %zu/%zu\n",
               failed_step + 1,
               sizeof(g_full_configs) / sizeof(g_full_configs[0]));
    }
}

void example5_full_init(gps_t *gps) {
    printf("=== Example 5: 전체 초기화 (많은 설정) ===\n");

    ubx_init_async_start(
        gps,
        UBX_LAYER_RAM,
        g_full_configs,
        sizeof(g_full_configs) / sizeof(g_full_configs[0]),
        on_full_init_complete,
        NULL
    );

    // 진행 상황 모니터링
    size_t last_step = 0;
    while (1) {
        ubx_init_async_process(gps);

        ubx_init_state_t state = ubx_init_async_get_state(gps);
        if (state != UBX_INIT_STATE_RUNNING) {
            break;
        }

        // 진행률 표시
        size_t current_step = gps->ubx_init_ctx.current_step;
        if (current_step != last_step) {
            printf("Progress: %zu/%zu\n", current_step,
                   sizeof(g_full_configs) / sizeof(g_full_configs[0]));
            last_step = current_step;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * 예시 6: Flash에 저장 (재부팅 후에도 유지)
 */
void example6_save_to_flash(gps_t *gps) {
    printf("=== Example 6: Flash에 저장 ===\n");

    // Flash layer로 초기화
    ubx_init_async_start(
        gps,
        UBX_LAYER_FLASH,  // ← Flash에 저장
        g_ublox_configs,
        sizeof(g_ublox_configs) / sizeof(g_ublox_configs[0]),
        on_init_complete,
        NULL
    );

    // Process 루프
    while (ubx_init_async_get_state(gps) == UBX_INIT_STATE_RUNNING) {
        ubx_init_async_process(gps);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    printf("Settings saved to Flash (persistent)\n");
}

/**
 * 예시 7: 사용자 데이터와 함께 콜백
 */
typedef struct {
    const char *device_name;
    int retry_count;
} init_user_data_t;

static void on_init_with_user_data(bool success, size_t failed_step, void *user_data) {
    init_user_data_t *data = (init_user_data_t *)user_data;

    if (success) {
        printf("✓ [%s] Init completed\n", data->device_name);
    } else {
        printf("✗ [%s] Init failed at step %zu, retrying (%d)...\n",
               data->device_name, failed_step, ++data->retry_count);
    }
}

void example7_user_data(gps_t *gps) {
    printf("=== Example 7: 사용자 데이터와 콜백 ===\n");

    static init_user_data_t user_data = {
        .device_name = "u-blox F9P",
        .retry_count = 0,
    };

    ubx_init_async_start(
        gps,
        UBX_LAYER_RAM,
        g_ublox_configs,
        sizeof(g_ublox_configs) / sizeof(g_ublox_configs[0]),
        on_init_with_user_data,
        &user_data  // ← 사용자 데이터
    );

    while (ubx_init_async_get_state(gps) == UBX_INIT_STATE_RUNNING) {
        ubx_init_async_process(gps);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
