#include "lora_to_gps_bridge.h"
#include "gps_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

#ifndef TAG
#define TAG "LORA_GPS_BRIDGE"
#endif

#include "log.h"

// Buffer pool for memory management
typedef struct {
  uint8_t data[LORA_GPS_BUFFER_SIZE];
  bool in_use;
} buffer_pool_entry_t;

// Bridge context
typedef struct {
  gps_id_t gps_id;                              // Target GPS ID
  QueueHandle_t packet_queue;                   // Packet queue
  SemaphoreHandle_t stats_mutex;                // Stats protection
  SemaphoreHandle_t pool_mutex;                 // Buffer pool protection
  TaskHandle_t forward_task;                    // Forwarding task handle
  lora_gps_stats_t stats;                       // Statistics
  buffer_pool_entry_t buffer_pool[LORA_GPS_BUFFER_POOL_SIZE];  // Buffer pool
  bool initialized;
} lora_gps_bridge_ctx_t;

static lora_gps_bridge_ctx_t g_bridge_ctx = {0};

// Forward declarations
static void lora_gps_forward_task(void *param);
static uint8_t *alloc_buffer(void);
static void free_buffer(uint8_t *buffer);

/**
 * @brief Allocate buffer from pool
 */
static uint8_t *alloc_buffer(void) {
  uint8_t *result = NULL;

  if (g_bridge_ctx.pool_mutex) {
    xSemaphoreTake(g_bridge_ctx.pool_mutex, portMAX_DELAY);
  }

  // Find free buffer
  for (int i = 0; i < LORA_GPS_BUFFER_POOL_SIZE; i++) {
    if (!g_bridge_ctx.buffer_pool[i].in_use) {
      g_bridge_ctx.buffer_pool[i].in_use = true;
      result = g_bridge_ctx.buffer_pool[i].data;
      break;
    }
  }

  if (g_bridge_ctx.pool_mutex) {
    xSemaphoreGive(g_bridge_ctx.pool_mutex);
  }

  if (!result) {
    // Update stats
    if (g_bridge_ctx.stats_mutex) {
      xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
    }
    g_bridge_ctx.stats.alloc_failures++;
    if (g_bridge_ctx.stats_mutex) {
      xSemaphoreGive(g_bridge_ctx.stats_mutex);
    }
  }

  return result;
}

/**
 * @brief Free buffer back to pool
 */
static void free_buffer(uint8_t *buffer) {
  if (!buffer) return;

  if (g_bridge_ctx.pool_mutex) {
    xSemaphoreTake(g_bridge_ctx.pool_mutex, portMAX_DELAY);
  }

  // Find and mark as free
  for (int i = 0; i < LORA_GPS_BUFFER_POOL_SIZE; i++) {
    if (g_bridge_ctx.buffer_pool[i].data == buffer) {
      g_bridge_ctx.buffer_pool[i].in_use = false;
      break;
    }
  }

  if (g_bridge_ctx.pool_mutex) {
    xSemaphoreGive(g_bridge_ctx.pool_mutex);
  }
}

/**
 * @brief Initialize bridge
 */
bool lora_gps_bridge_init(gps_id_t gps_id) {
  if (g_bridge_ctx.initialized) {
    LOG_WARN("Bridge already initialized");
    return false;
  }

  memset(&g_bridge_ctx, 0, sizeof(g_bridge_ctx));
  g_bridge_ctx.gps_id = gps_id;

  // Create queue
  g_bridge_ctx.packet_queue = xQueueCreate(LORA_GPS_QUEUE_SIZE, sizeof(lora_gps_packet_t));
  if (!g_bridge_ctx.packet_queue) {
    LOG_ERROR("Failed to create packet queue");
    return false;
  }

  // Create mutexes
  g_bridge_ctx.stats_mutex = xSemaphoreCreateMutex();
  g_bridge_ctx.pool_mutex = xSemaphoreCreateMutex();

  if (!g_bridge_ctx.stats_mutex || !g_bridge_ctx.pool_mutex) {
    LOG_ERROR("Failed to create mutexes");
    lora_gps_bridge_deinit();
    return false;
  }

  // Initialize buffer pool
  for (int i = 0; i < LORA_GPS_BUFFER_POOL_SIZE; i++) {
    g_bridge_ctx.buffer_pool[i].in_use = false;
  }

  // Create forwarding task
  BaseType_t result = xTaskCreate(
    lora_gps_forward_task,
    "lora_gps_fwd",
    512,                        // Stack size
    NULL,
    tskIDLE_PRIORITY + 3,       // Higher priority for low latency
    &g_bridge_ctx.forward_task
  );

  if (result != pdPASS) {
    LOG_ERROR("Failed to create forwarding task");
    lora_gps_bridge_deinit();
    return false;
  }

  g_bridge_ctx.initialized = true;
  LOG_INFO("Bridge initialized (GPS ID: %d, Queue: %d, Buffers: %d)",
           gps_id, LORA_GPS_QUEUE_SIZE, LORA_GPS_BUFFER_POOL_SIZE);

  return true;
}

/**
 * @brief Send data to bridge (non-blocking)
 */
bool lora_gps_bridge_send(const uint8_t *data, size_t len) {
  if (!g_bridge_ctx.initialized) {
    LOG_WARN("Bridge not initialized");
    return false;
  }

  if (!data || len == 0 || len > LORA_GPS_BUFFER_SIZE) {
    LOG_WARN("Invalid data or length (%u bytes)", len);
    return false;
  }

  // Allocate buffer from pool
  uint8_t *buffer = alloc_buffer();
  if (!buffer) {
    LOG_WARN("No buffer available, dropping packet (%u bytes)", len);

    // Update stats
    if (g_bridge_ctx.stats_mutex) {
      xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
    }
    g_bridge_ctx.stats.packets_dropped++;
    if (g_bridge_ctx.stats_mutex) {
      xSemaphoreGive(g_bridge_ctx.stats_mutex);
    }

    return false;
  }

  // Copy data to buffer
  memcpy(buffer, data, len);

  // Create packet
  lora_gps_packet_t packet = {
    .data = buffer,
    .len = len,
    .timestamp = xTaskGetTickCount()
  };

  // Send to queue (non-blocking)
  if (xQueueSend(g_bridge_ctx.packet_queue, &packet, 0) != pdTRUE) {
    LOG_WARN("Queue full, dropping packet (%u bytes)", len);

    // Free buffer
    free_buffer(buffer);

    // Update stats
    if (g_bridge_ctx.stats_mutex) {
      xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
    }
    g_bridge_ctx.stats.packets_dropped++;
    if (g_bridge_ctx.stats_mutex) {
      xSemaphoreGive(g_bridge_ctx.stats_mutex);
    }

    return false;
  }

  // Update stats
  if (g_bridge_ctx.stats_mutex) {
    xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
  }
  g_bridge_ctx.stats.packets_received++;
  g_bridge_ctx.stats.bytes_received += len;

  // Update queue high water mark
  UBaseType_t queue_count = uxQueueMessagesWaiting(g_bridge_ctx.packet_queue);
  if (queue_count > g_bridge_ctx.stats.queue_high_water) {
    g_bridge_ctx.stats.queue_high_water = queue_count;
  }

  if (g_bridge_ctx.stats_mutex) {
    xSemaphoreGive(g_bridge_ctx.stats_mutex);
  }

  return true;
}

/**
 * @brief Forwarding task - sends data to GPS
 */
static void lora_gps_forward_task(void *param) {
  lora_gps_packet_t packet;

  LOG_INFO("Forwarding task started");

  while (1) {
    // Wait for packet
    if (xQueueReceive(g_bridge_ctx.packet_queue, &packet, portMAX_DELAY) == pdTRUE) {
      if (!packet.data || packet.len == 0) {
        LOG_ERROR("Invalid packet received");
        continue;
      }

      // Send to GPS (uses mutex-protected gps_uart2_send internally)
      int result = gps_send_raw_data(g_bridge_ctx.gps_id, packet.data, packet.len);

      if (result == 0) {
        // Update stats
        if (g_bridge_ctx.stats_mutex) {
          xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
        }
        g_bridge_ctx.stats.packets_sent++;
        g_bridge_ctx.stats.bytes_sent += packet.len;
        if (g_bridge_ctx.stats_mutex) {
          xSemaphoreGive(g_bridge_ctx.stats_mutex);
        }

        LOG_DEBUG("Forwarded %u bytes to GPS[%d]", packet.len, g_bridge_ctx.gps_id);
      } else {
        LOG_ERROR("Failed to send to GPS[%d]", g_bridge_ctx.gps_id);
      }

      // Free buffer
      free_buffer(packet.data);
    }
  }
}

/**
 * @brief Get statistics
 */
void lora_gps_bridge_get_stats(lora_gps_stats_t *stats) {
  if (!stats) return;

  if (g_bridge_ctx.stats_mutex) {
    xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
  }

  memcpy(stats, &g_bridge_ctx.stats, sizeof(lora_gps_stats_t));

  if (g_bridge_ctx.stats_mutex) {
    xSemaphoreGive(g_bridge_ctx.stats_mutex);
  }
}

/**
 * @brief Reset statistics
 */
void lora_gps_bridge_reset_stats(void) {
  if (g_bridge_ctx.stats_mutex) {
    xSemaphoreTake(g_bridge_ctx.stats_mutex, portMAX_DELAY);
  }

  memset(&g_bridge_ctx.stats, 0, sizeof(lora_gps_stats_t));

  if (g_bridge_ctx.stats_mutex) {
    xSemaphoreGive(g_bridge_ctx.stats_mutex);
  }
}

/**
 * @brief Get queue count
 */
uint32_t lora_gps_bridge_get_queue_count(void) {
  if (!g_bridge_ctx.packet_queue) {
    return 0;
  }

  return (uint32_t)uxQueueMessagesWaiting(g_bridge_ctx.packet_queue);
}

/**
 * @brief Deinitialize bridge
 */
void lora_gps_bridge_deinit(void) {
  if (g_bridge_ctx.forward_task) {
    vTaskDelete(g_bridge_ctx.forward_task);
    g_bridge_ctx.forward_task = NULL;
  }

  if (g_bridge_ctx.packet_queue) {
    // Drain queue and free buffers
    lora_gps_packet_t packet;
    while (xQueueReceive(g_bridge_ctx.packet_queue, &packet, 0) == pdTRUE) {
      if (packet.data) {
        free_buffer(packet.data);
      }
    }

    vQueueDelete(g_bridge_ctx.packet_queue);
    g_bridge_ctx.packet_queue = NULL;
  }

  if (g_bridge_ctx.stats_mutex) {
    vSemaphoreDelete(g_bridge_ctx.stats_mutex);
    g_bridge_ctx.stats_mutex = NULL;
  }

  if (g_bridge_ctx.pool_mutex) {
    vSemaphoreDelete(g_bridge_ctx.pool_mutex);
    g_bridge_ctx.pool_mutex = NULL;
  }

  g_bridge_ctx.initialized = false;
  LOG_INFO("Bridge deinitialized");
}
