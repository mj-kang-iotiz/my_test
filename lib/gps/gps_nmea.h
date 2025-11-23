#ifndef GPS_NMEA_H
#define GPS_NMEA_H

#include "gps_types.h"
#include <stdint.h>
#include <stdbool.h>

#define GPS_NMEA_TERM_SIZE 13

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

/**
 * @brief GGA quality fix 상태
 *
 */
typedef enum {
  GPS_FIX_INVALID = 0,
  GPS_FIX_GPS = 1,
  GPS_FIX_DGPS = 2,
  GPS_FIX_PPS = 3,
  GPS_FIX_RTK_FIX = 4,
  GPS_FIX_RTK_FLOAT = 5,
  GPS_FIX_DR = 6
} gps_fix_t;

/**
 * @brief GGA 데이터
 *
 * @note 패킷 예시
 * $xxGGA,time,lat,NS,lon,EW,quality,numSV,HDOP,alt,altUnit,sep,sepUnit,diffAge,diffStation*cs\r\n
 * @note 패킷 예시
 * $GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B\r\n
 */
typedef struct {
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  double lat;
  char ns;
  double lon;
  char ew;
  gps_fix_t fix;
  uint8_t sat_num;
  double hdop;
  double alt;
  double geo_sep;
} gps_gga_t;

/**
 * @brief 파싱한 NMEA 183 프로토콜 데이터
 *
 */
typedef struct {
  gps_gga_t gga;
  char gga_raw[100];
  uint8_t gga_raw_pos;
  bool gga_is_rdy;
} gps_nmea_data_t;

/**
 * @brief NMEA 파싱에 필요한 변수
 *
 */
typedef struct {
  char term_str[GPS_NMEA_TERM_SIZE];
  uint8_t term_pos;
  uint8_t term_num;

  gps_nmea_msg_t msg_type;
  uint8_t crc;
  uint8_t star;
} gps_nmea_parser_t;

typedef struct gps_s gps_t;

uint8_t gps_parse_nmea_term(gps_t *gps);

#endif
