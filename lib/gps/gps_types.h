#ifndef GPS_TYPES_H
#define GPS_TYPES_H

#include <stdint.h>

/**
 * @brief 처리중인 GPS 프로토콜 종류 및 이벤트
 *
 */
typedef enum {
  GPS_PROTOCOL_NONE = 0,
  GPS_PROTOCOL_NMEA = 1,
  GPS_PROTOCOL_UBX = 2,
  GPS_PROTOCOL_UNICORE = 3,

  /* 초기화 이벤트 (100번대) */
  GPS_EVENT_READY = 100,      // RDY 수신
  GPS_EVENT_ACK_OK = 101,     // ACK 수신 성공
  GPS_EVENT_ACK_FAIL = 102,   // ACK 수신 실패

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

#endif