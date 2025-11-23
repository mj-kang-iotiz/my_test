#include "gps_ubx.h"
#include "gps.h"
#include "gps_parse.h"
#include <string.h>

static inline void calc_ubx_chksum(gps_t *gps);
static inline uint8_t check_ubx_chksum(gps_t *gps);
static void store_ubx_nav_data(gps_t *gps);
static void store_ubx_data(gps_t *gps);

/**
 * @brief ubx 프로토콜 체크섬 계산
 *
 * @param[inout] gps
 */
static inline void calc_ubx_chksum(gps_t *gps) {
  gps->ubx.cal_chksum_a = 0;
  gps->ubx.cal_chksum_b = 0;

  for (int i = 0; i < gps->ubx.len + 4; i++) {
    gps->ubx.cal_chksum_a += gps->payload[i];
    gps->ubx.cal_chksum_b += gps->ubx.cal_chksum_a;
  }
}

/**
 * @brief ubx 프로토콜 체크섬 확인
 *
 * @param[in] gps
 * @return uint8_t 1: success, 0: fail
 */
static inline uint8_t check_ubx_chksum(gps_t *gps) {
  calc_ubx_chksum(gps);

  if (gps->ubx.cal_chksum_a == gps->ubx.chksum_a &&
      gps->ubx.cal_chksum_b == gps->ubx.chksum_b) {
    return 1;
  }

  return 0;
}

/**
 * @brief 파싱한 ubx nav 프토토콜 데이터 저장
 *
 * @param[inout] gps
 */
static void store_ubx_nav_data(gps_t *gps) {
  switch (gps->ubx.id) {
  case GPS_UBX_NAV_ID_HPPOSLLH:
    memcpy(&gps->ubx_data.hpposllh, &gps->payload[4],
           sizeof(gps_ubx_nav_hpposllh_t));
    break;

  default:
    break;
  }
}

/**
 * @brief 파싱한 ubx 프로토콜 데이터 저장
 *
 * @param[inout] gps
 */
static void store_ubx_data(gps_t *gps) {
  switch (gps->ubx.class) {
  case GPS_UBX_CLASS_NAV:
    store_ubx_nav_data(gps);
    break;

  default:
    break;
  }
}

/**
 * @brief ubx 프로토콜 파싱
 *
 * @param[inout] gps
 * @return uint8_t 1: success 0: checksum mismatch
 */
uint8_t gps_parse_ubx(gps_t *gps) {
  if (gps->pos == 1) {
    gps->ubx.class = gps->payload[0];
    gps->state = GPS_PARSE_STATE_UBX_MSG_CLASS;
  } else if (gps->pos == 2) {
    gps->ubx.id = gps->payload[1];
    gps->state = GPS_PARSE_STATE_UBX_MSG_ID;
  } else if (gps->pos == 4) {
    gps->ubx.len = (gps->payload[2] | ((gps->payload[3] << 8)));
    gps->state = GPS_PARSE_STATE_UBX_LEN;
  } else {
    if (gps->pos <= 4 + gps->ubx.len) {
      gps->state = GPS_PARSE_STATE_UBX_PAYLOAD;
    } else if (gps->pos == 5 + gps->ubx.len) {
      gps->state = GPS_PARSE_STATE_UBX_CHKSUM_A;
      gps->ubx.chksum_a = gps->payload[4 + gps->ubx.len];
    } else if (gps->pos == 6 + gps->ubx.len) {
      gps->state = GPS_PARSE_STATE_UBX_CHKSUM_B;
      gps->ubx.chksum_b = gps->payload[5 + gps->ubx.len];

      if (check_ubx_chksum(gps)) {
        store_ubx_data(gps);
        gps->protocol = GPS_PROTOCOL_NONE;
        gps->state = GPS_PARSE_STATE_NONE;

        return 1;
      } else {
        gps->protocol = GPS_PROTOCOL_NONE;
        gps->state = GPS_PARSE_STATE_NONE;
        return 0;
      }
    }
  }

  return 1;
}