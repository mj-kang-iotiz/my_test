#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/**
 * ============================================================================
 * PCB 버전 선택
 * ============================================================================
 * 빌드시 -DBOARD_VERSION=1 형태로 지정 또는 여기서 직접 설정
 */
#ifndef BOARD_VERSION
#define BOARD_VERSION 1  // 기본값: Board V1
#endif

/**
 * ============================================================================
 * GPS 타입 정의
 * ============================================================================
 */
#define GPS_TYPE_UNICORE  1
#define GPS_TYPE_UBLOX    2

/**
 * ============================================================================
 * GPS 역할 정의
 * ============================================================================
 */
#define GPS_ROLE_BASE     1
#define GPS_ROLE_ROVER    2

/**
 * ============================================================================
 * 보드별 설정
 * ============================================================================
 */

#if BOARD_VERSION == 1
  /* ========== Board V1: Unicore Base ========== */
  #define BOARD_NAME          "UNICORE-BASE-V1"
  #define BOARD_DESCRIPTION   "Unicore GPS Base Station"

  // GPS 설정
  #define GPS_TYPE            GPS_TYPE_UNICORE
  #define GPS_ROLE            GPS_ROLE_BASE
  #define GPS_COUNT           1

  // GPS 인스턴스 활성화
  #define GPS_BASE_ENABLE     1
  #define GPS_ROVER_ENABLE    0
  #define GPS_ROVER2_ENABLE   0  // Rover 두 번째 GPS

  // 모듈 활성화
  #define USE_LTE             1
  #define USE_LORA            0
  #define USE_RS485           1

  // LED 설정
  #define LED_COUNT           3

#elif BOARD_VERSION == 2
  /* ========== Board V2: Unicore Rover ========== */
  #define BOARD_NAME          "UNICORE-ROVER-V1"
  #define BOARD_DESCRIPTION   "Unicore GPS Rover"

  // GPS 설정
  #define GPS_TYPE            GPS_TYPE_UNICORE
  #define GPS_ROLE            GPS_ROLE_ROVER
  #define GPS_COUNT           1

  // GPS 인스턴스 활성화
  #define GPS_BASE_ENABLE     0
  #define GPS_ROVER_ENABLE    1
  #define GPS_ROVER2_ENABLE   0

  // 모듈 활성화
  #define USE_LTE             1
  #define USE_LORA            0
  #define USE_RS485           0  // Rover에는 RS485 없음

  // LED 설정
  #define LED_COUNT           3

#elif BOARD_VERSION == 3
  /* ========== Board V3: Ublox Base (F9P) ========== */
  #define BOARD_NAME          "UBLOX-BASE-V1"
  #define BOARD_DESCRIPTION   "Ublox F9P GPS Base Station"

  // GPS 설정
  #define GPS_TYPE            GPS_TYPE_UBLOX
  #define GPS_ROLE            GPS_ROLE_BASE
  #define GPS_COUNT           1

  // GPS 인스턴스 활성화
  #define GPS_BASE_ENABLE     1
  #define GPS_ROVER_ENABLE    0
  #define GPS_ROVER2_ENABLE   0

  // 모듈 활성화
  #define USE_LTE             1
  #define USE_LORA            0
  #define USE_RS485           1

  // LED 설정
  #define LED_COUNT           3

#elif BOARD_VERSION == 4
  /* ========== Board V4: Ublox Rover (Dual F9P) ========== */
  #define BOARD_NAME          "UBLOX-ROVER-DUAL-V1"
  #define BOARD_DESCRIPTION   "Ublox Dual F9P GPS Rover"

  // GPS 설정
  #define GPS_TYPE            GPS_TYPE_UBLOX
  #define GPS_ROLE            GPS_ROLE_ROVER
  #define GPS_COUNT           2  // ✅ F9P 2개

  // GPS 인스턴스 활성화 (Rover에 2개)
  #define GPS_BASE_ENABLE     0
  #define GPS_ROVER_ENABLE    1   // Rover 첫 번째 F9P
  #define GPS_ROVER2_ENABLE   1   // Rover 두 번째 F9P

  // 모듈 활성화
  #define USE_LTE             1
  #define USE_LORA            0
  #define USE_RS485           0

  // LED 설정
  #define LED_COUNT           3

#else
  #error "Invalid BOARD_VERSION! Must be 1-4"
#endif

/**
 * ============================================================================
 * GPS ID 정의 (보드 독립적)
 * ============================================================================
 */
typedef enum {
  GPS_ID_BASE = 0,    // Base 또는 Rover 첫 번째
  GPS_ID_ROVER = 1,   // Rover 두 번째 (V4만 해당)
  GPS_ID_MAX
} gps_id_t;

/**
 * ============================================================================
 * 컴파일 타임 체크
 * ============================================================================
 */
#if GPS_COUNT > GPS_ID_MAX
  #error "GPS_COUNT exceeds GPS_ID_MAX!"
#endif

#if GPS_ROVER2_ENABLE && (GPS_COUNT < 2)
  #error "GPS_ROVER2_ENABLE requires GPS_COUNT >= 2"
#endif

#endif // BOARD_CONFIG_H
