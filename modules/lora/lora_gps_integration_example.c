/**
 * @file lora_gps_integration_example.c
 * @brief LoRa P2P 수신 데이터를 GPS로 전달하는 통합 예제
 *
 * 이 예제는 다음 시나리오를 구현합니다:
 * 1. LoRa P2P로 RTCM 데이터 수신 (매초 ~1KB)
 * 2. 수신한 데이터를 GPS 모듈로 전달
 * 3. GPS UART 송신 충돌 방지 (뮤텍스 사용)
 * 4. 별도 태스크로 처리하여 LoRa 수신 콜백 블로킹 방지
 */

#include "lora.h"
#include "lora_to_gps_bridge.h"
#include "gps_app.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

// LoRa instance
static lora_t g_lora;

/**
 * @brief LoRa P2P 수신 콜백
 *
 * LoRa로 데이터를 수신하면 호출됩니다.
 * 이 콜백은 빠르게 반환되어야 하므로, 데이터를 브릿지로 전달만 합니다.
 */
static void lora_p2p_rx_handler(lora_t *lora, lora_p2p_rx_data_t *rx_data) {
  printf("\n[LoRa] P2P data received: %u bytes, RSSI: %d dBm, SNR: %d dB\n",
         rx_data->len, rx_data->rssi, rx_data->snr);

  // 브릿지로 전달 (논블로킹)
  bool result = lora_gps_bridge_send(rx_data->data, rx_data->len);

  if (result) {
    printf("[Bridge] Data queued successfully (%u bytes)\n", rx_data->len);
  } else {
    printf("[Bridge] Failed to queue data (queue full or no buffer)\n");
  }
}

/**
 * @brief 시스템 초기화
 */
void lora_gps_system_init(void) {
  printf("\n=== LoRa-GPS Integration System Init ===\n");

  // 1. GPS 초기화 (먼저 해야 함)
  printf("1. Initializing GPS...\n");
  gps_init_all();
  vTaskDelay(pdMS_TO_TICKS(1000));

  // 2. LoRa 초기화
  printf("2. Initializing LoRa...\n");
  lora_init(&g_lora, &huart3);  // UART3 for LoRa

  // 3. LoRa P2P 수신 콜백 등록
  printf("3. Setting LoRa P2P RX callback...\n");
  lora_set_p2p_rx_callback(&g_lora, lora_p2p_rx_handler);

  // 4. LoRa-to-GPS 브릿지 초기화
  printf("4. Initializing LoRa-to-GPS bridge...\n");
  bool bridge_ok = lora_gps_bridge_init(GPS_ID_BASE);  // GPS_ID_BASE로 전송
  if (bridge_ok) {
    printf("   Bridge initialized successfully!\n");
  } else {
    printf("   Bridge initialization failed!\n");
    return;
  }

  // 5. LoRa P2P 모드 설정
  printf("5. Configuring LoRa P2P mode...\n");
  bool result = lora_set_p2p_mode(&g_lora, true);
  if (result) {
    printf("   P2P mode enabled\n");
  } else {
    printf("   Failed to set P2P mode\n");
  }

  // 6. P2P 파라미터 설정
  printf("6. Configuring P2P parameters...\n");
  lora_p2p_config_t config = {
    .frequency = 922000000,        // 922 MHz
    .spreading_factor = 7,         // SF7
    .bandwidth = 0,                // 125 kHz
    .coding_rate = 1,              // 4/5
    .preamble_length = 8,          // 8 symbols
    .tx_power = 14                 // 14 dBm
  };
  result = lora_configure_p2p(&g_lora, &config);
  if (result) {
    printf("   P2P configured: 922MHz, SF7, BW125, CR4/5\n");
  } else {
    printf("   Failed to configure P2P\n");
  }

  // 7. 연속 수신 모드 시작
  printf("7. Starting continuous receive mode...\n");
  result = lora_p2p_start_rx(&g_lora, 0);  // 0 = continuous
  if (result) {
    printf("   Receive mode started\n");
  } else {
    printf("   Failed to start receive mode\n");
  }

  printf("\n=== System Ready ===\n");
  printf("Waiting for LoRa P2P data...\n");
  printf("Received data will be forwarded to GPS module.\n\n");
}

/**
 * @brief 통계 출력 태스크
 *
 * 주기적으로 브릿지 통계를 출력합니다.
 */
void lora_gps_stats_task(void *param) {
  lora_gps_stats_t stats;

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));  // 5초마다

    lora_gps_bridge_get_stats(&stats);
    uint32_t queue_count = lora_gps_bridge_get_queue_count();

    printf("\n=== Bridge Statistics ===\n");
    printf("Packets: RX=%lu, TX=%lu, Dropped=%lu\n",
           stats.packets_received, stats.packets_sent, stats.packets_dropped);
    printf("Bytes:   RX=%lu, TX=%lu\n",
           stats.bytes_received, stats.bytes_sent);
    printf("Queue:   Current=%lu, HighWater=%lu/%d\n",
           queue_count, stats.queue_high_water, LORA_GPS_QUEUE_SIZE);
    printf("Buffers: AllocFail=%lu\n", stats.alloc_failures);
    printf("========================\n\n");
  }
}

/**
 * @brief 메인 애플리케이션 시작
 */
void lora_gps_app_start(void) {
  // 시스템 초기화
  lora_gps_system_init();

  // 통계 출력 태스크 생성 (선택사항)
  xTaskCreate(lora_gps_stats_task, "lora_gps_stats", 256, NULL,
              tskIDLE_PRIORITY + 1, NULL);

  printf("Application started!\n");
}

/**
 * @brief 수동 테스트: LoRa P2P 데이터 전송
 *
 * 다른 장치에서 이 함수를 호출하여 테스트 데이터를 전송할 수 있습니다.
 */
void lora_gps_test_send(void) {
  // 테스트 RTCM 데이터 (예제)
  uint8_t test_data[100];
  for (int i = 0; i < 100; i++) {
    test_data[i] = i;
  }

  printf("Sending test data (%d bytes)...\n", sizeof(test_data));
  bool result = lora_p2p_send(&g_lora, test_data, sizeof(test_data));

  if (result) {
    printf("Test data sent successfully\n");
  } else {
    printf("Failed to send test data\n");
  }
}

/**
 * @brief 시나리오별 사용 예제
 */

// 예제 1: 기본 사용
void example_basic_usage(void) {
  // GPS 초기화
  gps_init_all();

  // LoRa 초기화
  lora_init(&g_lora, &huart3);
  lora_set_p2p_rx_callback(&g_lora, lora_p2p_rx_handler);

  // 브릿지 초기화
  lora_gps_bridge_init(GPS_ID_BASE);

  // LoRa P2P 설정 및 수신 시작
  lora_set_p2p_mode(&g_lora, true);
  lora_p2p_config_t config = {
    .frequency = 922000000,
    .spreading_factor = 7,
    .bandwidth = 0,
    .coding_rate = 1,
    .preamble_length = 8,
    .tx_power = 14
  };
  lora_configure_p2p(&g_lora, &config);
  lora_p2p_start_rx(&g_lora, 0);

  // 이제 LoRa로 수신한 데이터가 자동으로 GPS로 전달됩니다
}

// 예제 2: 통계 모니터링
void example_with_monitoring(void) {
  // 기본 설정
  example_basic_usage();

  // 주기적으로 통계 확인
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10000));

    lora_gps_stats_t stats;
    lora_gps_bridge_get_stats(&stats);

    printf("Throughput: %.2f KB/s\n",
           (float)stats.bytes_received / 10.0 / 1024.0);

    // 큐가 80% 이상 차면 경고
    uint32_t queue_count = lora_gps_bridge_get_queue_count();
    if (queue_count > LORA_GPS_QUEUE_SIZE * 0.8) {
      printf("WARNING: Queue almost full (%lu/%d)\n",
             queue_count, LORA_GPS_QUEUE_SIZE);
    }
  }
}

// 예제 3: 에러 처리
void example_with_error_handling(void) {
  lora_gps_stats_t stats;
  lora_gps_stats_t prev_stats = {0};

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    lora_gps_bridge_get_stats(&stats);

    // 패킷 드롭 체크
    if (stats.packets_dropped > prev_stats.packets_dropped) {
      printf("ERROR: %lu packets dropped!\n",
             stats.packets_dropped - prev_stats.packets_dropped);
      printf("       Queue size: %d, Consider increasing LORA_GPS_QUEUE_SIZE\n",
             LORA_GPS_QUEUE_SIZE);
    }

    // 버퍼 할당 실패 체크
    if (stats.alloc_failures > prev_stats.alloc_failures) {
      printf("ERROR: %lu buffer allocation failures!\n",
             stats.alloc_failures - prev_stats.alloc_failures);
      printf("       Buffer pool size: %d, Consider increasing LORA_GPS_BUFFER_POOL_SIZE\n",
             LORA_GPS_BUFFER_POOL_SIZE);
    }

    prev_stats = stats;
  }
}

/**
 * @brief 성능 튜닝 가이드
 *
 * 매초 1KB 수신 시:
 * - LORA_GPS_QUEUE_SIZE: 20 (충분함, 최대 20초 버퍼링)
 * - LORA_GPS_BUFFER_POOL_SIZE: 10 (충분함)
 * - Bridge task priority: tskIDLE_PRIORITY + 3 (높은 우선순위)
 *
 * 더 높은 throughput이 필요한 경우:
 * - LORA_GPS_BUFFER_SIZE 증가 (현재 1024)
 * - LORA_GPS_QUEUE_SIZE 증가
 * - LORA_GPS_BUFFER_POOL_SIZE 증가
 */
