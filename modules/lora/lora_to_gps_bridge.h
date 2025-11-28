#ifndef LORA_TO_GPS_BRIDGE_H
#define LORA_TO_GPS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "gps_app.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration
#define LORA_GPS_QUEUE_SIZE         20      // Queue depth
#define LORA_GPS_BUFFER_SIZE        1024    // Max packet size
#define LORA_GPS_BUFFER_POOL_SIZE   10      // Number of buffers in pool

// Data packet structure
typedef struct {
  uint8_t *data;                // Pointer to data buffer
  size_t len;                   // Data length
  uint32_t timestamp;           // Reception timestamp (tick count)
} lora_gps_packet_t;

// Bridge statistics
typedef struct {
  uint32_t packets_received;    // Total packets received from LoRa
  uint32_t packets_sent;        // Total packets sent to GPS
  uint32_t packets_dropped;     // Packets dropped (queue full)
  uint32_t bytes_received;      // Total bytes received
  uint32_t bytes_sent;          // Total bytes sent
  uint32_t queue_high_water;    // Max queue usage
  uint32_t alloc_failures;      // Buffer allocation failures
} lora_gps_stats_t;

/**
 * @brief Initialize LoRa-to-GPS bridge
 *
 * Creates queue, buffer pool, and forwarding task.
 * Must be called after GPS initialization.
 *
 * @param gps_id Target GPS module ID
 * @return true if successful, false otherwise
 */
bool lora_gps_bridge_init(gps_id_t gps_id);

/**
 * @brief Send data from LoRa to GPS
 *
 * This is a non-blocking call. Data is queued and sent by bridge task.
 * Can be called from LoRa receive callback.
 *
 * @param data Pointer to received data
 * @param len Data length (max LORA_GPS_BUFFER_SIZE)
 * @return true if queued successfully, false if queue full or buffer unavailable
 */
bool lora_gps_bridge_send(const uint8_t *data, size_t len);

/**
 * @brief Get bridge statistics
 *
 * @param stats Pointer to statistics structure
 */
void lora_gps_bridge_get_stats(lora_gps_stats_t *stats);

/**
 * @brief Reset statistics counters
 */
void lora_gps_bridge_reset_stats(void);

/**
 * @brief Get current queue usage
 *
 * @return Number of packets waiting in queue
 */
uint32_t lora_gps_bridge_get_queue_count(void);

/**
 * @brief Deinitialize bridge (cleanup resources)
 */
void lora_gps_bridge_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // LORA_TO_GPS_BRIDGE_H
