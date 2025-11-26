#ifndef GPS_TYPES_H
#define GPS_TYPES_H

#include <stdint.h>
#include "gps_config.h"

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

typedef enum {
  GPS_EVENT_NONE = 0,

  /* Unicore 초기화 이벤트 */
  GPS_EVENT_READY,        // RDY 수신 (Unicore)
  GPS_EVENT_ACK_OK,       // ACK 수신 성공 (Unicore)
  GPS_EVENT_ACK_FAIL,     // ACK 수신 실패 (Unicore)

  /* 공통 데이터 이벤트 */
  GPS_EVENT_DATA_PARSED,  // 프로토콜 데이터 파싱 완료

  GPS_EVENT_INVALID = UINT8_MAX
} gps_event_t;

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
 *
 */
typedef enum {
  GPS_NMEA_MSG_NONE = 0,
  GPS_NMEA_MSG_GGA = 1,
  GPS_NMEA_MSG_RMC = 2,
  GPS_NMEA_MSG_INVALID = UINT8_MAX
} gps_nmea_msg_t;


typedef union
{
    gps_nmea_msg_t nmea;
    struct
    {
        uint8_t class;
        uint8_t id;
    }ubx;
}gps_msg_t;

#endif
