#ifndef GPS_UNICORE_H
#define GPS_UNICORE_H

#include "gps_types.h"
#include <stdint.h>
#include <stdbool.h>

#define GPS_UNICORE_TERM_SIZE 32

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
 * @brief Unicore 파서 상태
 */
typedef struct {
  char term_str[GPS_UNICORE_TERM_SIZE];
  uint8_t term_pos;
  uint8_t term_num;

  gps_unicore_msg_t msg_type;
  uint8_t crc;
  uint8_t star;
} gps_unicore_parser_t;

typedef struct gps_s gps_t;

uint8_t gps_parse_unicore_term(gps_t *gps);

#endif
