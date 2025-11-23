#ifndef GPS_UBX_H
#define GPS_UBX_H

#include "gps_types.h"
#include <stdint.h>

/**
 * @brief ubx 프로토콜 클래스 타입
 *
 */
typedef enum {
  GPS_UBX_CLASS_NONE = 0,
  GPS_UBX_CLASS_NAV = 0x01,
} gps_ubx_class_t;

/**
 * @brief ubx 프로토콜 NAV 클래스 메시지 id
 *
 */
typedef enum {
  GPS_UBX_NAV_ID_NONE = 0,
  GPS_UBX_NAV_ID_HPPOSLLH = 0x14,
} gps_ubx_nav_id_t;

/**
 * @brief ubx 프로토콜 NAV 클래스 HPPOSLLH 메시지
 *
 */
typedef struct {
  uint8_t version; // 0x00
  uint8_t reserved[2];
  uint8_t flag; // 0: valid 1: invalid
  uint32_t tow; // time of week [ms]
  int32_t lon;
  int32_t lat;
  int32_t height;   // [mm]
  int32_t msl;      // [mm]
  int8_t lon_hp;    // 1e-9 단위 / deg * 1e-7 = lon + (lon_hp * 1e-2)
  int8_t lat_hp;    // 1e-9 단위 / deg * 1e-7 = lat + (lat_hp * 1e-2)
  int8_t height_hp; // 0.1mm 단위 / height + height_hp * 0.1 [mm]
  int8_t msl_hp;    // 0.1mm 단위 / msl + msl_hp * 0.1 [mm]
  uint32_t hacc;    // 0.1mm 단위
  uint32_t vacc;    // 0.1mm 단위
} gps_ubx_nav_hpposllh_t;

/**
 * @brief UBX 파싱에 필요한 변수
 *
 */
typedef struct {
  uint8_t class;
  uint8_t id;
  uint16_t len;     // 2byte
  uint8_t chksum_a; // 1byte
  uint8_t chksum_b; // 1byte

  uint8_t cal_chksum_a;
  uint8_t cal_chksum_b;
} gps_ubx_parser_t;

/**
 * @brief UBX 프로토콜 데이터
 *
 */
typedef struct {
  gps_ubx_nav_hpposllh_t hpposllh;
} gps_ubx_data_t;

typedef struct gps_s gps_t;

uint8_t gps_parse_ubx(gps_t *gps);

#endif
