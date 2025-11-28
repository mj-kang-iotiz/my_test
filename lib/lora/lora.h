#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// RAK4270 AT Commands
typedef enum {
  LORA_CMD_AT = 0,           // AT
  LORA_CMD_RESET,            // AT+RESET
  LORA_CMD_SET_MODE,         // AT+MODE=0 (P2P) or AT+MODE=1 (LoRaWAN)
  LORA_CMD_GET_MODE,         // AT+MODE=?
  LORA_CMD_SET_P2P_CONFIG,   // AT+P2P=freq:SF:BW:CR:PRlen:PWR
  LORA_CMD_GET_P2P_CONFIG,   // AT+P2P=?
  LORA_CMD_P2P_SEND,         // AT+PSEND=data
  LORA_CMD_P2P_RX_MODE,      // AT+PRECV=time (0=continuous)
  LORA_CMD_P2P_STOP_RX,      // AT+PRECV=0
  LORA_CMD_GET_VERSION,      // AT+VER=?
  LORA_CMD_UNKNOWN
} lora_cmd_t;

// Response types
typedef enum {
  LORA_RESP_NONE = 0,
  LORA_RESP_OK,
  LORA_RESP_ERROR,
  LORA_RESP_RECV_DATA,       // +EVT:RXP2P:-rssi:snr:len:data
  LORA_RESP_TX_DONE,         // +EVT:TXP2P DONE
  LORA_RESP_TX_TIMEOUT,      // +EVT:TXP2P TIMEOUT
  LORA_RESP_RX_TIMEOUT,      // +EVT:RXP2P RECEIVE TIMEOUT
  LORA_RESP_UNKNOWN
} lora_response_t;

// P2P Configuration
typedef struct {
  uint32_t frequency;        // 150000000 - 960000000 Hz
  uint8_t spreading_factor;  // 6-12
  uint8_t bandwidth;         // 0=125kHz, 1=250kHz, 2=500kHz
  uint8_t coding_rate;       // 1=4/5, 2=4/6, 3=4/7, 4=4/8
  uint16_t preamble_length;  // 2-65535
  uint8_t tx_power;          // 5-22 dBm
} lora_p2p_config_t;

// Received P2P data
typedef struct {
  int16_t rssi;              // RSSI value
  int16_t snr;               // SNR value
  uint16_t len;              // Data length
  uint8_t data[256];         // Received data
} lora_p2p_rx_data_t;

// Forward declaration
typedef struct lora_s lora_t;

// AT command callback for async operations
typedef void (*lora_at_cmd_handler)(lora_t *lora, lora_cmd_t cmd, void *msg, bool is_ok);

// P2P data receive callback
typedef void (*lora_p2p_rx_handler)(lora_t *lora, lora_p2p_rx_data_t *rx_data);

// AT Command message structure
typedef struct {
  lora_cmd_t cmd;                    // Command type
  char params[128];                  // Command parameters
  lora_at_cmd_handler callback;      // Async callback (NULL for sync)
  SemaphoreHandle_t sem;             // Sync semaphore (NULL for async)
  uint32_t timeout_ms;               // Command timeout
  lora_response_t response;          // Response type
  char response_data[256];           // Response data
  bool is_ok;                        // True if OK, false if ERROR
} lora_at_cmd_t;

// LoRa module status
typedef struct {
  bool is_initialized;
  bool is_p2p_mode;
  bool is_receiving;
  lora_response_t last_response;
  bool is_ok;
  bool is_err;
  lora_p2p_config_t p2p_config;
} lora_status_t;

// LoRa receive buffer
typedef struct {
  char data[512];                    // Receive buffer
  size_t len;                        // Current length
} lora_recv_t;

// Main LoRa structure
typedef struct lora_s {
  // UART handle
  void *uart_handle;                 // USART_HandleTypeDef pointer

  // Command handling
  lora_at_cmd_t *current_cmd;        // Current executing command
  SemaphoreHandle_t cmd_mutex;       // Mutex for current_cmd access
  SemaphoreHandle_t producer_sem;    // Producer synchronization
  QueueHandle_t at_cmd_queue;        // Command queue

  // Status and receive buffer
  lora_status_t status;
  lora_recv_t recv;

  // P2P receive callback
  lora_p2p_rx_handler p2p_rx_callback;

  // Tasks
  TaskHandle_t producer_task;
  TaskHandle_t parser_task;
} lora_t;

// Initialization and deinitialization
void lora_init(lora_t *lora, void *uart_handle);
void lora_deinit(lora_t *lora);

// Synchronous AT command functions
bool lora_send_at_cmd_sync(lora_t *lora, lora_cmd_t cmd, const char *params,
                           char *response, size_t response_size, uint32_t timeout_ms);

// Asynchronous AT command functions
bool lora_send_at_cmd_async(lora_t *lora, lora_cmd_t cmd, const char *params,
                            lora_at_cmd_handler callback, uint32_t timeout_ms);

// High-level P2P functions
bool lora_set_p2p_mode(lora_t *lora, bool enable);
bool lora_configure_p2p(lora_t *lora, const lora_p2p_config_t *config);
bool lora_p2p_send(lora_t *lora, const uint8_t *data, size_t len);
bool lora_p2p_start_rx(lora_t *lora, uint32_t timeout_ms);
bool lora_p2p_stop_rx(lora_t *lora);

// Set P2P receive callback
void lora_set_p2p_rx_callback(lora_t *lora, lora_p2p_rx_handler callback);

// UART receive processing (called from UART ISR or DMA callback)
void lora_uart_rx_process(lora_t *lora, const uint8_t *data, size_t len);

// Parser task (runs in FreeRTOS task)
void lora_parser_task(void *param);
void lora_producer_task(void *param);

#ifdef __cplusplus
}
#endif

#endif // LORA_H
