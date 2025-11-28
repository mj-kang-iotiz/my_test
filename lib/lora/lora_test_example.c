#include "lora.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

// LoRa instance
static lora_t g_lora;

// Asynchronous command callback example
static void lora_async_callback(lora_t *lora, lora_cmd_t cmd, void *msg, bool is_ok) {
  printf("Async callback - Command: %d, Result: %s\n", cmd, is_ok ? "OK" : "ERROR");

  if (is_ok) {
    printf("Command completed successfully!\n");
  } else {
    printf("Command failed!\n");
  }
}

// P2P receive callback
static void lora_p2p_rx_callback(lora_t *lora, lora_p2p_rx_data_t *rx_data) {
  printf("\n=== P2P Data Received ===\n");
  printf("RSSI: %d dBm\n", rx_data->rssi);
  printf("SNR: %d dB\n", rx_data->snr);
  printf("Length: %u bytes\n", rx_data->len);
  printf("Data (hex): ");
  for (uint16_t i = 0; i < rx_data->len; i++) {
    printf("%02X ", rx_data->data[i]);
  }
  printf("\n");
  printf("Data (ASCII): ");
  for (uint16_t i = 0; i < rx_data->len; i++) {
    if (rx_data->data[i] >= 32 && rx_data->data[i] < 127) {
      printf("%c", rx_data->data[i]);
    } else {
      printf(".");
    }
  }
  printf("\n========================\n");
}

// Initialize LoRa module
void lora_test_init(void) {
  printf("Initializing LoRa module...\n");
  lora_init(&g_lora, &huart3);  // UART3 for LoRa

  // Set P2P receive callback
  lora_set_p2p_rx_callback(&g_lora, lora_p2p_rx_callback);

  printf("LoRa module initialized!\n");
}

// Test synchronous commands
void lora_test_sync_commands(void) {
  printf("\n=== Testing Synchronous Commands ===\n");

  char response[128];
  bool result;

  // Test 1: AT command
  printf("\n1. Testing AT command (sync)...\n");
  result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_AT, NULL,
                                 response, sizeof(response), 5000);
  printf("Result: %s\n", result ? "OK" : "ERROR");
  printf("Response: %s\n", response);

  vTaskDelay(pdMS_TO_TICKS(500));

  // Test 2: Get version
  printf("\n2. Getting firmware version (sync)...\n");
  result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_GET_VERSION, NULL,
                                 response, sizeof(response), 5000);
  printf("Result: %s\n", result ? "OK" : "ERROR");
  printf("Response: %s\n", response);

  vTaskDelay(pdMS_TO_TICKS(500));

  // Test 3: Set P2P mode
  printf("\n3. Setting P2P mode (sync)...\n");
  result = lora_set_p2p_mode(&g_lora, true);
  printf("Result: %s\n", result ? "OK" : "ERROR");

  vTaskDelay(pdMS_TO_TICKS(500));

  // Test 4: Get current mode
  printf("\n4. Getting current mode (sync)...\n");
  result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_GET_MODE, NULL,
                                 response, sizeof(response), 5000);
  printf("Result: %s\n", result ? "OK" : "ERROR");
  printf("Response: %s\n", response);

  printf("\n=== Sync Commands Test Complete ===\n");
}

// Test asynchronous commands
void lora_test_async_commands(void) {
  printf("\n=== Testing Asynchronous Commands ===\n");

  bool result;

  // Test 1: AT command (async)
  printf("\n1. Testing AT command (async)...\n");
  result = lora_send_at_cmd_async(&g_lora, LORA_CMD_AT, NULL,
                                  lora_async_callback, 5000);
  printf("Command sent: %s\n", result ? "Success" : "Failed");
  printf("Waiting for callback...\n");

  vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for callback

  // Test 2: Get version (async)
  printf("\n2. Getting firmware version (async)...\n");
  result = lora_send_at_cmd_async(&g_lora, LORA_CMD_GET_VERSION, NULL,
                                  lora_async_callback, 5000);
  printf("Command sent: %s\n", result ? "Success" : "Failed");
  printf("Waiting for callback...\n");

  vTaskDelay(pdMS_TO_TICKS(2000));

  printf("\n=== Async Commands Test Complete ===\n");
}

// Configure and test P2P
void lora_test_p2p_config(void) {
  printf("\n=== Configuring P2P ===\n");

  // P2P configuration
  lora_p2p_config_t config = {
    .frequency = 922000000,        // 922 MHz
    .spreading_factor = 7,         // SF7
    .bandwidth = 0,                // 125 kHz
    .coding_rate = 1,              // 4/5
    .preamble_length = 8,          // 8 symbols
    .tx_power = 14                 // 14 dBm
  };

  printf("Configuration:\n");
  printf("  Frequency: %lu Hz\n", config.frequency);
  printf("  SF: %u\n", config.spreading_factor);
  printf("  BW: %u (125kHz)\n", config.bandwidth);
  printf("  CR: 4/%u\n", config.coding_rate + 4);
  printf("  Preamble: %u\n", config.preamble_length);
  printf("  TX Power: %u dBm\n", config.tx_power);

  bool result = lora_configure_p2p(&g_lora, &config);
  printf("Configuration result: %s\n", result ? "OK" : "ERROR");

  vTaskDelay(pdMS_TO_TICKS(500));

  // Get current P2P configuration
  printf("\nGetting current P2P config...\n");
  char response[128];
  result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_GET_P2P_CONFIG, NULL,
                                 response, sizeof(response), 5000);
  printf("Result: %s\n", result ? "OK" : "ERROR");
  printf("Response: %s\n", response);

  printf("\n=== P2P Configuration Complete ===\n");
}

// Test P2P send
void lora_test_p2p_send(void) {
  printf("\n=== Testing P2P Send ===\n");

  // Send test message
  const char *test_msg = "Hello LoRa!";
  printf("Sending message: %s\n", test_msg);

  bool result = lora_p2p_send(&g_lora, (const uint8_t *)test_msg, strlen(test_msg));
  printf("Send result: %s\n", result ? "OK" : "ERROR");

  printf("\n=== P2P Send Test Complete ===\n");
}

// Test P2P receive
void lora_test_p2p_receive(void) {
  printf("\n=== Testing P2P Receive ===\n");

  // Start continuous receive mode
  printf("Starting continuous receive mode...\n");
  bool result = lora_p2p_start_rx(&g_lora, 0);  // 0 = continuous
  printf("Start RX result: %s\n", result ? "OK" : "ERROR");

  if (result) {
    printf("Receiving... (waiting for data)\n");
    printf("Data will be displayed via callback when received.\n");
    printf("This will run continuously. To stop, call lora_p2p_stop_rx().\n");
  }

  printf("\n=== P2P Receive Started ===\n");
}

// Stop P2P receive
void lora_test_p2p_stop_receive(void) {
  printf("\n=== Stopping P2P Receive ===\n");

  bool result = lora_p2p_stop_rx(&g_lora);
  printf("Stop RX result: %s\n", result ? "OK" : "ERROR");

  printf("\n=== P2P Receive Stopped ===\n");
}

// Full test sequence
void lora_test_full_sequence(void) {
  printf("\n========================================\n");
  printf("RAK4270 LoRa P2P Full Test Sequence\n");
  printf("========================================\n");

  // Initialize
  lora_test_init();
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Test sync commands
  lora_test_sync_commands();
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Test async commands
  lora_test_async_commands();
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Configure P2P
  lora_test_p2p_config();
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Test send
  lora_test_p2p_send();
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Test receive (will run continuously)
  lora_test_p2p_receive();

  printf("\n========================================\n");
  printf("Test sequence complete!\n");
  printf("Receive mode is active. Send data to see it received.\n");
  printf("========================================\n");
}

// Example: How to use in your main application
void lora_example_usage(void) {
  // Option 1: Run full test sequence
  lora_test_full_sequence();

  // Option 2: Use individual functions as needed
  /*
  lora_test_init();

  // Synchronous command example
  char response[128];
  bool result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_AT, NULL,
                                      response, sizeof(response), 5000);
  if (result) {
    printf("AT command OK: %s\n", response);
  } else {
    printf("AT command ERROR\n");
  }

  // Asynchronous command example
  result = lora_send_at_cmd_async(&g_lora, LORA_CMD_GET_VERSION, NULL,
                                  lora_async_callback, 5000);

  // Configure and start P2P receive
  lora_p2p_config_t config = {
    .frequency = 922000000,
    .spreading_factor = 7,
    .bandwidth = 0,
    .coding_rate = 1,
    .preamble_length = 8,
    .tx_power = 14
  };
  lora_configure_p2p(&g_lora, &config);
  lora_p2p_start_rx(&g_lora, 0);  // Continuous receive
  */
}
