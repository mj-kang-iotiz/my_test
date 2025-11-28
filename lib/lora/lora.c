#include "lora.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define LORA_CMD_QUEUE_SIZE 10
#define LORA_PRODUCER_TASK_STACK_SIZE 512
#define LORA_PARSER_TASK_STACK_SIZE 512
#define LORA_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

// Helper function prototypes
static void lora_send_raw_at_cmd(lora_t *lora, const char *cmd);
static void lora_parse_response(lora_t *lora);
static void lora_parse_process(lora_t *lora, const void *data, size_t len);
static const char *lora_cmd_to_string(lora_cmd_t cmd, const char *params);

// Initialize LoRa module
void lora_init(lora_t *lora, void *uart_handle) {
  memset(lora, 0, sizeof(lora_t));

  lora->uart_handle = uart_handle;

  // Create synchronization objects
  lora->cmd_mutex = xSemaphoreCreateMutex();
  lora->producer_sem = xSemaphoreCreateBinary();
  lora->at_cmd_queue = xQueueCreate(LORA_CMD_QUEUE_SIZE, sizeof(lora_at_cmd_t));

  // Initialize status
  lora->status.is_initialized = true;
  lora->status.is_p2p_mode = false;
  lora->status.is_receiving = false;

  // Create tasks
  xTaskCreate(lora_producer_task, "lora_prod", LORA_PRODUCER_TASK_STACK_SIZE,
              lora, LORA_TASK_PRIORITY, &lora->producer_task);

  xTaskCreate(lora_parser_task, "lora_parse", LORA_PARSER_TASK_STACK_SIZE,
              lora, LORA_TASK_PRIORITY, &lora->parser_task);
}

// Deinitialize LoRa module
void lora_deinit(lora_t *lora) {
  if (lora->producer_task) {
    vTaskDelete(lora->producer_task);
    lora->producer_task = NULL;
  }

  if (lora->parser_task) {
    vTaskDelete(lora->parser_task);
    lora->parser_task = NULL;
  }

  if (lora->cmd_mutex) {
    vSemaphoreDelete(lora->cmd_mutex);
    lora->cmd_mutex = NULL;
  }

  if (lora->producer_sem) {
    vSemaphoreDelete(lora->producer_sem);
    lora->producer_sem = NULL;
  }

  if (lora->at_cmd_queue) {
    vQueueDelete(lora->at_cmd_queue);
    lora->at_cmd_queue = NULL;
  }

  lora->status.is_initialized = false;
}

// Synchronous AT command
bool lora_send_at_cmd_sync(lora_t *lora, lora_cmd_t cmd, const char *params,
                           char *response, size_t response_size, uint32_t timeout_ms) {
  if (!lora || !lora->status.is_initialized) {
    return false;
  }

  lora_at_cmd_t at_cmd = {0};
  at_cmd.cmd = cmd;
  at_cmd.callback = NULL;
  at_cmd.sem = xSemaphoreCreateBinary();
  at_cmd.timeout_ms = timeout_ms;

  if (params) {
    strncpy(at_cmd.params, params, sizeof(at_cmd.params) - 1);
  }

  // Send to queue
  if (xQueueSend(lora->at_cmd_queue, &at_cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
    vSemaphoreDelete(at_cmd.sem);
    return false;
  }

  // Wait for response
  bool result = false;
  if (xSemaphoreTake(at_cmd.sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
    // Need to get response from current_cmd after it's processed
    // For simplicity, we'll use a different approach
    result = at_cmd.is_ok;

    if (response && response_size > 0) {
      strncpy(response, at_cmd.response_data, response_size - 1);
      response[response_size - 1] = '\0';
    }
  }

  vSemaphoreDelete(at_cmd.sem);
  return result;
}

// Asynchronous AT command
bool lora_send_at_cmd_async(lora_t *lora, lora_cmd_t cmd, const char *params,
                            lora_at_cmd_handler callback, uint32_t timeout_ms) {
  if (!lora || !lora->status.is_initialized || !callback) {
    return false;
  }

  lora_at_cmd_t at_cmd = {0};
  at_cmd.cmd = cmd;
  at_cmd.callback = callback;
  at_cmd.sem = NULL;
  at_cmd.timeout_ms = timeout_ms;

  if (params) {
    strncpy(at_cmd.params, params, sizeof(at_cmd.params) - 1);
  }

  // Send to queue
  if (xQueueSend(lora->at_cmd_queue, &at_cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return false;
  }

  return true;
}

// Set P2P receive callback
void lora_set_p2p_rx_callback(lora_t *lora, lora_p2p_rx_handler callback) {
  if (lora) {
    lora->p2p_rx_callback = callback;
  }
}

// Convert command to AT string
static const char *lora_cmd_to_string(lora_cmd_t cmd, const char *params) {
  static char cmd_buf[256];

  switch (cmd) {
    case LORA_CMD_AT:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT\r\n");
      break;
    case LORA_CMD_RESET:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+RESET\r\n");
      break;
    case LORA_CMD_SET_MODE:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+MODE=%s\r\n", params ? params : "0");
      break;
    case LORA_CMD_GET_MODE:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+MODE=?\r\n");
      break;
    case LORA_CMD_SET_P2P_CONFIG:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+P2P=%s\r\n", params ? params : "");
      break;
    case LORA_CMD_GET_P2P_CONFIG:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+P2P=?\r\n");
      break;
    case LORA_CMD_P2P_SEND:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+PSEND=%s\r\n", params ? params : "");
      break;
    case LORA_CMD_P2P_RX_MODE:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+PRECV=%s\r\n", params ? params : "0");
      break;
    case LORA_CMD_P2P_STOP_RX:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+PRECV=0\r\n");
      break;
    case LORA_CMD_GET_VERSION:
      snprintf(cmd_buf, sizeof(cmd_buf), "AT+VER=?\r\n");
      break;
    default:
      cmd_buf[0] = '\0';
      break;
  }

  return cmd_buf;
}

// Send raw AT command
static void lora_send_raw_at_cmd(lora_t *lora, const char *cmd) {
  if (!lora || !lora->uart_handle || !cmd) {
    return;
  }

  UART_HandleTypeDef *uart = (UART_HandleTypeDef *)lora->uart_handle;
  HAL_UART_Transmit(uart, (uint8_t *)cmd, strlen(cmd), 1000);
}

// Producer task - sends commands
void lora_producer_task(void *param) {
  lora_t *lora = (lora_t *)param;
  lora_at_cmd_t cmd_msg;

  while (1) {
    // Wait for command from queue
    if (xQueueReceive(lora->at_cmd_queue, &cmd_msg, portMAX_DELAY) == pdTRUE) {
      // Take mutex to set current_cmd
      xSemaphoreTake(lora->cmd_mutex, portMAX_DELAY);

      // Allocate and copy command
      lora->current_cmd = pvPortMalloc(sizeof(lora_at_cmd_t));
      if (lora->current_cmd) {
        memcpy(lora->current_cmd, &cmd_msg, sizeof(lora_at_cmd_t));

        // Clear status flags
        lora->status.is_ok = false;
        lora->status.is_err = false;

        // Send AT command
        const char *at_str = lora_cmd_to_string(cmd_msg.cmd, cmd_msg.params);
        lora_send_raw_at_cmd(lora, at_str);
      }

      xSemaphoreGive(lora->cmd_mutex);

      // Wait for response (parser will signal this)
      xSemaphoreTake(lora->producer_sem, pdMS_TO_TICKS(cmd_msg.timeout_ms));
    }
  }
}

// Parse response line
static void lora_parse_response(lora_t *lora) {
  if (!lora || !lora->current_cmd) {
    return;
  }

  char *line = lora->recv.data;

  // Check for OK response
  if (strncmp(line, "OK", 2) == 0) {
    lora->status.is_ok = true;
    lora->status.is_err = false;

    // Take mutex to access current_cmd
    xSemaphoreTake(lora->cmd_mutex, portMAX_DELAY);

    if (lora->current_cmd) {
      // Backup command info
      SemaphoreHandle_t caller_sem = lora->current_cmd->sem;
      lora_at_cmd_handler callback = lora->current_cmd->callback;
      lora_cmd_t cmd = lora->current_cmd->cmd;

      lora->current_cmd->is_ok = true;
      lora->current_cmd->response = LORA_RESP_OK;

      // Copy response data if needed
      strncpy(lora->current_cmd->response_data, line,
              sizeof(lora->current_cmd->response_data) - 1);

      // Free current command
      vPortFree(lora->current_cmd);
      lora->current_cmd = NULL;

      xSemaphoreGive(lora->cmd_mutex);

      // Wake producer first
      xSemaphoreGive(lora->producer_sem);

      // Then notify caller or invoke callback
      if (caller_sem) {
        xSemaphoreGive(caller_sem);  // Synchronous
      } else if (callback) {
        callback(lora, cmd, NULL, true);  // Asynchronous
      }
    } else {
      xSemaphoreGive(lora->cmd_mutex);
    }

    return;
  }

  // Check for ERROR response
  if (strncmp(line, "ERROR", 5) == 0 || strncmp(line, "AT_ERROR", 8) == 0 ||
      strncmp(line, "AT_PARAM_ERROR", 14) == 0) {
    lora->status.is_ok = false;
    lora->status.is_err = true;

    xSemaphoreTake(lora->cmd_mutex, portMAX_DELAY);

    if (lora->current_cmd) {
      SemaphoreHandle_t caller_sem = lora->current_cmd->sem;
      lora_at_cmd_handler callback = lora->current_cmd->callback;
      lora_cmd_t cmd = lora->current_cmd->cmd;

      lora->current_cmd->is_ok = false;
      lora->current_cmd->response = LORA_RESP_ERROR;

      strncpy(lora->current_cmd->response_data, line,
              sizeof(lora->current_cmd->response_data) - 1);

      vPortFree(lora->current_cmd);
      lora->current_cmd = NULL;

      xSemaphoreGive(lora->cmd_mutex);

      xSemaphoreGive(lora->producer_sem);

      if (caller_sem) {
        xSemaphoreGive(caller_sem);
      } else if (callback) {
        callback(lora, cmd, NULL, false);
      }
    } else {
      xSemaphoreGive(lora->cmd_mutex);
    }

    return;
  }

  // Check for P2P receive event: +EVT:RXP2P:-rssi:snr:len:data
  if (strncmp(line, "+EVT:RXP2P:", 11) == 0) {
    lora_p2p_rx_data_t rx_data = {0};

    // Parse: +EVT:RXP2P:-45:8:5:48656C6C6F
    char *ptr = line + 11;  // Skip "+EVT:RXP2P:"

    // Parse RSSI (negative value)
    rx_data.rssi = (int16_t)atoi(ptr);

    // Find next ':'
    ptr = strchr(ptr, ':');
    if (ptr) {
      ptr++;
      rx_data.snr = (int16_t)atoi(ptr);

      ptr = strchr(ptr, ':');
      if (ptr) {
        ptr++;
        rx_data.len = (uint16_t)atoi(ptr);

        ptr = strchr(ptr, ':');
        if (ptr) {
          ptr++;
          // Parse hex data
          size_t data_idx = 0;
          while (*ptr && data_idx < sizeof(rx_data.data) && data_idx < rx_data.len) {
            char hex_byte[3] = {ptr[0], ptr[1], '\0'};
            rx_data.data[data_idx++] = (uint8_t)strtol(hex_byte, NULL, 16);
            ptr += 2;
          }
        }
      }
    }

    // Call P2P receive callback
    if (lora->p2p_rx_callback) {
      lora->p2p_rx_callback(lora, &rx_data);
    }

    return;
  }

  // Check for TX done event
  if (strncmp(line, "+EVT:TXP2P DONE", 15) == 0) {
    lora->status.last_response = LORA_RESP_TX_DONE;
    return;
  }

  // Other responses can be handled here
}

// Parser task - processes UART data
void lora_parser_task(void *param) {
  // This task will be notified by UART ISR
  // For now, it's a placeholder
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Process received UART data
void lora_uart_rx_process(lora_t *lora, const uint8_t *data, size_t len) {
  if (!lora || !data || len == 0) {
    return;
  }

  lora_parse_process(lora, data, len);
}

// Parse incoming data character by character
static void lora_parse_process(lora_t *lora, const void *data, size_t len) {
  const uint8_t *buf = (const uint8_t *)data;
  static char ch_prev = 0;

  for (size_t i = 0; i < len; i++) {
    char ch = (char)buf[i];

    // Add to receive buffer
    if (ch != '\r' && ch != '\n') {
      if (lora->recv.len < sizeof(lora->recv.data) - 1) {
        lora->recv.data[lora->recv.len++] = ch;
        lora->recv.data[lora->recv.len] = '\0';
      }
    }

    // Check for complete line (\r\n)
    if (ch_prev == '\r' && ch == '\n') {
      // Parse the complete line
      lora_parse_response(lora);

      // Clear receive buffer
      memset(lora->recv.data, 0, sizeof(lora->recv.data));
      lora->recv.len = 0;
    }

    ch_prev = ch;
  }
}

// High-level functions
bool lora_set_p2p_mode(lora_t *lora, bool enable) {
  char params[8];
  snprintf(params, sizeof(params), "%d", enable ? 0 : 1);  // 0=P2P, 1=LoRaWAN

  char response[64] = {0};
  bool result = lora_send_at_cmd_sync(lora, LORA_CMD_SET_MODE, params,
                                      response, sizeof(response), 5000);

  if (result) {
    lora->status.is_p2p_mode = enable;
  }

  return result;
}

bool lora_configure_p2p(lora_t *lora, const lora_p2p_config_t *config) {
  if (!lora || !config) {
    return false;
  }

  // Format: AT+P2P=freq:SF:BW:CR:PRlen:PWR
  char params[128];
  snprintf(params, sizeof(params), "%lu:%u:%u:%u:%u:%u",
           config->frequency,
           config->spreading_factor,
           config->bandwidth,
           config->coding_rate,
           config->preamble_length,
           config->tx_power);

  char response[64] = {0};
  bool result = lora_send_at_cmd_sync(lora, LORA_CMD_SET_P2P_CONFIG, params,
                                      response, sizeof(response), 5000);

  if (result) {
    lora->status.p2p_config = *config;
  }

  return result;
}

bool lora_p2p_send(lora_t *lora, const uint8_t *data, size_t len) {
  if (!lora || !data || len == 0 || len > 256) {
    return false;
  }

  // Convert data to hex string
  char hex_str[512];
  for (size_t i = 0; i < len; i++) {
    snprintf(hex_str + (i * 2), 3, "%02X", data[i]);
  }

  char response[64] = {0};
  return lora_send_at_cmd_sync(lora, LORA_CMD_P2P_SEND, hex_str,
                               response, sizeof(response), 10000);
}

bool lora_p2p_start_rx(lora_t *lora, uint32_t timeout_ms) {
  if (!lora) {
    return false;
  }

  char params[16];
  if (timeout_ms == 0) {
    snprintf(params, sizeof(params), "0");  // Continuous mode
  } else {
    snprintf(params, sizeof(params), "%lu", timeout_ms);
  }

  char response[64] = {0};
  bool result = lora_send_at_cmd_sync(lora, LORA_CMD_P2P_RX_MODE, params,
                                      response, sizeof(response), 5000);

  if (result) {
    lora->status.is_receiving = true;
  }

  return result;
}

bool lora_p2p_stop_rx(lora_t *lora) {
  if (!lora) {
    return false;
  }

  char response[64] = {0};
  bool result = lora_send_at_cmd_sync(lora, LORA_CMD_P2P_STOP_RX, NULL,
                                      response, sizeof(response), 5000);

  if (result) {
    lora->status.is_receiving = false;
  }

  return result;
}
