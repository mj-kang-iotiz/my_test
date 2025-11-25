#ifndef GPS_TYPES_H
#define GPS_TYPES_H

#include <stdint.h>

/**
 * @brief 처리중인 GPS 프로토콜 종류
 *
 */
typedef enum {
  GPS_PROTOCOL_NONE = 0,
  GPS_PROTOCOL_NMEA = 1,
  GPS_PROTOCOL_UBX = 2,
  GPS_PROTOCOL_UNICORE = 3,
  GPS_PROTOCOL_INVALID = UINT8_MAX
} gps_procotol_t;

/**
 * @brief GPS 파싱 상태
 *
 */
typedef enum {
  GPS_PARSE_STATE_NONE = 0,

  /* NMEA 183 protocol */
  GPS_PARSE_STATE_NMEA_START = 1,
  GPS_PARSE_STATE_NMEA_ADDR = 2,
  GPS_PARSE_STATE_NMEA_DATA = 3,
  GPS_PARSE_STATE_NMEA_CHKSUM = 4,
  GPS_PARSE_STATE_NMEA_END_SEQ = 5,

  /* UBX protocol*/
  GPS_PARSE_STATE_UBX_SYNC_1 = 10,
  GPS_PARSE_STATE_UBX_SYNC_2 = 11,
  GPS_PARSE_STATE_UBX_MSG_CLASS = 12,
  GPS_PARSE_STATE_UBX_MSG_ID = 13,
  GPS_PARSE_STATE_UBX_LEN = 14,
  GPS_PARSE_STATE_UBX_PAYLOAD = 15,
  GPS_PARSE_STATE_UBX_CHKSUM_A = 16,
  GPS_PARSE_STATE_UBX_CHKSUM_B = 17,

  /* UNICORE protocol*/

  GPS_PARSE_STATE_INVALID = UINT8_MAX
} gps_parse_state_t;

/**
 * @brief NMEA183 프로토콜 sentence
 * @note NMEA 전용 타입이지만, gps_msg_t union에서 타입 안전성과
 *       IDE 자동완성을 위해 gps_types.h에 위치 (순환 헤더 의존성 방지)
 */
typedef enum {
  GPS_NMEA_MSG_NONE = 0,
  GPS_NMEA_MSG_GGA = 1,
  GPS_NMEA_MSG_RMC = 2,
  GPS_NMEA_MSG_INVALID = UINT8_MAX
} gps_nmea_msg_t;

/**
 * @brief GPS 메시지 타입 (프로토콜별 구분)
 *
 */
typedef union {
  gps_nmea_msg_t nmea;  // NMEA 메시지 타입 (GPS_NMEA_MSG_GGA, GPS_NMEA_MSG_RMC)
  struct {
    uint8_t class;   // UBX 메시지 클래스 (GPS_UBX_CLASS_NAV)
    uint8_t id;      // UBX 메시지 ID (GPS_UBX_NAV_ID_HPPOSLLH)
  } ubx;
  uint16_t raw;      // 직접 접근용
} gps_msg_t;

#endif