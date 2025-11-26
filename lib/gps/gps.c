#include "gps.h"
#include "parser.h"
#include <string.h>

static inline void add_nmea_chksum(gps_t *gps, char ch);
static inline uint8_t check_nmea_chksum(gps_t *gps);
static inline void term_add(gps_t *gps, char ch);
static inline void term_next(gps_t *gps);

void _gps_gga_raw_add(gps_t *gps, char ch) {
  if (gps->nmea_data.gga_raw_pos < 99) {
    gps->nmea_data.gga_raw[gps->nmea_data.gga_raw_pos] = ch;
    gps->nmea_data.gga_raw[++gps->nmea_data.gga_raw_pos] = '\0';
  }
}

bool get_gga(gps_t *gps, char* buf, uint8_t* len)
{
  bool ret = false;

  xSemaphoreTake(gps->mutex, portMAX_DELAY);
  if(gps->nmea_data.gga_is_rdy && gps->nmea_data.gga.fix != GPS_FIX_INVALID)
  {
    strncpy(buf, gps->nmea_data.gga_raw, gps->nmea_data.gga_raw_pos + 1);
    *len = gps->nmea_data.gga_raw_pos;
    ret = true;
  }
  xSemaphoreGive(gps->mutex);

  return ret;
}

/**
 * @brief NMEA 프로토콜 체크섬 추가
 *
 * @param[out] gps
 * @param[in] ch
 */
static inline void add_nmea_chksum(gps_t *gps, char ch) {
  gps->nmea.crc ^= (uint8_t)ch;
}

/**
 * @brief NMEA 프로토콜 체크섬 확인
 *
 * @param[in] gps
 * @return uint8_t 1: success 0: fail
 */
static inline uint8_t check_nmea_chksum(gps_t *gps) {
  uint8_t crc = 0;

  crc = (uint8_t)((((PARSER_CHAR_HEX_TO_NUM(gps->nmea.term_str[0])) & 0x0FU)
                   << 0x04U) |
                  ((PARSER_CHAR_HEX_TO_NUM(gps->nmea.term_str[1])) & 0x0FU));

  if (gps->nmea.crc != crc) {
    return 0;
  }

  return 1;
}

/**
 * @brief 프로토콜 페이로드 추가
 *
 * @param[inout] gps
 * @param[in] ch
 */
static inline void add_payload(gps_t *gps, char ch) {
  if (gps->pos < GPS_PAYLOAD_SIZE - 1) {
    gps->payload[gps->pos] = ch;
    gps->payload[++gps->pos] = 0;
  }
}

/**
 * @brief NMEA183 프로토콜 , 사이에 있는 문자 추가
 *
 * @param[inout] gps
 * @param[in] ch
 */
static inline void term_add(gps_t *gps, char ch) {
  if (gps->nmea.term_pos < GPS_NMEA_TERM_SIZE - 1) {
    gps->nmea.term_str[gps->nmea.term_pos] = ch;
    gps->nmea.term_str[++gps->nmea.term_pos] = 0;
  }
}

/**
 * @brief NMEA183 프로토콜 , 파싱후 초기화
 *
 * @param[out] gps
 */
static inline void term_next(gps_t *gps) {
  gps->nmea.term_str[0] = 0;
  gps->nmea.term_pos = 0;
  gps->nmea.term_num++;
}

/**
 * @brief gps 객체 초기화
 *
 * @param[out] gps
 */
void gps_init(gps_t *gps)
{
  memset(gps, 0, sizeof(*gps));
  gps->mutex = xSemaphoreCreateMutex();
  // HAL ops는 gps_port_init_instance()에서 설정됨
}

/**
 * @brief RDY 문자열 검색
 */
static bool check_for_rdy(const uint8_t *data, size_t len) {
  if (len < 3) return false;
  for (size_t i = 0; i <= len - 3; i++) {
    if (data[i] == 'R' && data[i+1] == 'D' && data[i+2] == 'Y') {
      return true;
    }
  }
  return false;
}

/**
 * @brief OK 문자열 검색
 */
static bool check_for_ok(const uint8_t *data, size_t len) {
  if (len < 2) return false;
  for (size_t i = 0; i <= len - 2; i++) {
    if (data[i] == 'O' && data[i+1] == 'K') {
      return true;
    }
  }
  return false;
}

/**
 * @brief GPS 프로토콜 파싱
 *
 * @param[inout] gps
 * @param[in] data
 * @param[in] len
 */
void gps_parse_process(gps_t *gps, const void *data, size_t len) {
  const uint8_t *d = data;

  // 초기화 중: RDY 검출 (데이터는 계속 파싱)
  if (gps->init_state == GPS_INIT_WAIT_READY) {
    if (check_for_rdy(d, len)) {
      gps->init_state = GPS_INIT_DONE;
      if (gps->handler) {
        gps->handler(gps, GPS_EVENT_READY, GPS_PROTOCOL_UNICORE, 0);
      }
      // return 제거: NMEA/UBX 데이터도 계속 파싱
    }
  }

  // NMEA/UBX 파싱

  for (; len > 0; ++d, --len) {
    /* @TODO GPS_PROTOCOL_NONE 일때 start 문자 찾는걸 만들고, 프로토콜에 따라
     * 다르게 파싱하게끔 만들기 */
    if (gps->protocol == GPS_PROTOCOL_NONE) {
      if (*d == '$') {
        memset(gps->nmea.term_str, 0, sizeof(gps->nmea.term_str));
        gps->nmea.term_pos = 0;
        gps->nmea.term_num = 0;
        memset(&gps->nmea, 0, sizeof(gps->nmea));

        gps->protocol = GPS_PROTOCOL_NMEA;
        gps->state = GPS_PARSE_STATE_NMEA_START;
      } else if (*d == 0xB5) {
        gps->state = GPS_PARSE_STATE_UBX_SYNC_1;
      } else if (*d == 0x62 && gps->state == GPS_PARSE_STATE_UBX_SYNC_1) {
        memset(gps->payload, 0, sizeof(gps->payload));
        memset(&gps->ubx, 0, sizeof(gps->ubx));
        gps->pos = 0;

        gps->protocol = GPS_PROTOCOL_UBX;
        gps->state = GPS_PARSE_STATE_UBX_SYNC_2;
      } else {
        gps->state = GPS_PARSE_STATE_NONE;
      }
    } else if (gps->protocol == GPS_PROTOCOL_NMEA) {
      if(gps->nmea.msg_type == GPS_NMEA_MSG_GGA)
      {
          _gps_gga_raw_add(gps, *d);
      }

      if (*d == ',') {
        gps_parse_nmea_term(gps);
        add_nmea_chksum(gps, *d);
        term_next(gps);
      } else if (*d == '*') {
        gps_parse_nmea_term(gps);
        gps->nmea.star = 1;
        term_next(gps);

        gps->state = GPS_PARSE_STATE_NMEA_CHKSUM;
      } else if (*d == '\r') {
        if (check_nmea_chksum(gps)) {
        }

        if(gps->nmea.msg_type == GPS_NMEA_MSG_GGA)
        {
            _gps_gga_raw_add(gps, *d);
            _gps_gga_raw_add(gps, '\n');
            gps->nmea_data.gga_is_rdy = true;
        }

        // NMEA 파싱 완료 이벤트 전달
        if (gps->handler) {
          gps->handler(gps, GPS_EVENT_DATA_PARSED, GPS_PROTOCOL_NMEA, gps->nmea.msg_type);
        }

        gps->protocol = GPS_PROTOCOL_NONE;
        gps->state = GPS_PARSE_STATE_NONE;
      } else {
        if (!gps->nmea.star) {
          add_nmea_chksum(gps, *d);
        }
        term_add(gps, *d);
      }
    } else if (gps->protocol == GPS_PROTOCOL_UBX) {
      add_payload(gps, *d);
      gps_parse_ubx(gps);
    }
  }
}

void gps_set_evt_handler(gps_t* gps, evt_handler handler)
{
  if(handler)
  {
    gps->handler = handler;
  }
}