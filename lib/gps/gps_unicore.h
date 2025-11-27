#ifndef GPS_UNICORE_H
#define GPS_UNICORE_H

#include "gps_types.h"
#include <stdint.h>
#include <stdbool.h>

#define GPS_UNICORE_TERM_SIZE 32
#define GPS_UNICORE_BIN_HEADER_SIZE 28
#define GPS_UNICORE_BIN_SYNC_1 0xAA
#define GPS_UNICORE_BIN_SYNC_2 0x44
#define GPS_UNICORE_BIN_SYNC_3 0x12

/**
 * @brief Unicore 응답 상태
 */
typedef enum {
  GPS_UNICORE_RESP_NONE = 0,
  GPS_UNICORE_RESP_OK = 1,
  GPS_UNICORE_RESP_ERROR = 2,
  GPS_UNICORE_RESP_UNKNOWN = 3
} gps_unicore_response_t;

/**
 * @brief Unicore 메시지 타입
 */
typedef enum {
  GPS_UNICORE_MSG_NONE = 0,
  GPS_UNICORE_MSG_COMMAND = 1,
  GPS_UNICORE_MSG_CONFIG = 2,
  GPS_UNICORE_MSG_INVALID = UINT8_MAX
} gps_unicore_msg_t;

/**
 * @brief Unicore 파싱 데이터
 */
typedef struct {
  gps_unicore_response_t last_response;
  char response_str[GPS_UNICORE_TERM_SIZE];
} gps_unicore_data_t;

/**
 * @brief Unicore ASCII 파서 상태
 */
typedef struct {
  char term_str[GPS_UNICORE_TERM_SIZE];
  uint8_t term_pos;
  uint8_t term_num;

  gps_unicore_msg_t msg_type;
  uint8_t crc;
  uint8_t star;
} gps_unicore_parser_t;

/**
 * @brief Unicore 바이너리 헤더
 */
typedef struct {
  uint8_t header_len;      // Header length (28)
  uint16_t msg_id;         // Message ID
  uint8_t msg_type;        // Message type
  uint8_t port_addr;       // Port address
  uint16_t msg_len;        // Message length (payload)
  uint16_t sequence;       // Sequence number
  uint8_t idle_time;       // Idle time
  uint8_t time_status;     // Time status
  uint16_t week;           // GPS week
  uint32_t milliseconds;   // GPS milliseconds
  uint32_t rcv_status;     // Receiver status
  uint16_t reserved;       // Reserved
  uint16_t sw_version;     // SW version
} gps_unicore_bin_header_t;

/**
 * @brief Unicore 바이너리 파서 상태
 */
typedef struct {
  gps_unicore_bin_header_t header;
  uint16_t payload_pos;    // 현재 페이로드 읽은 위치
  uint8_t header_pos;      // 헤더 읽은 위치
  uint32_t crc;            // CRC 누적
  uint8_t crc_bytes[4];    // CRC 바이트 (4바이트)
  uint8_t crc_pos;         // CRC 읽은 위치
} gps_unicore_bin_parser_t;

typedef struct gps_s gps_t;

uint8_t gps_parse_unicore_term(gps_t *gps);
void gps_parse_unicore_bin(gps_t *gps);

#endif
