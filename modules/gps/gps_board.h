#ifndef GPS_BOARD_H
#define GPS_BOARD_H

#include "board_config.h"

/**
 * @brief GPS 보드별 설정 자동 선택
 *
 * BOARD_VERSION에 따라 적절한 보드 설정 헤더를 포함
 */

#if BOARD_VERSION == 1
  #include "board/gps_board_v1.h"  // Unicore Base
#elif BOARD_VERSION == 2
  #include "board/gps_board_v2.h"  // Unicore Rover
#elif BOARD_VERSION == 3
  #include "board/gps_board_v3.h"  // Ublox Base (F9P)
#elif BOARD_VERSION == 4
  #include "board/gps_board_v4.h"  // Ublox Rover (Dual F9P)
#else
  #error "Invalid BOARD_VERSION for GPS module"
#endif

/**
 * @brief GPS 프로토콜 정의
 */
#define GPS_PROTOCOL_NMEA  1
#define GPS_PROTOCOL_UBX   2

#endif // GPS_BOARD_H
