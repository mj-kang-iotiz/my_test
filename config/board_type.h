#ifndef BOARD_TYPE_H
#define BOARD_TYPE_H

#define BOARD_VERSION "v0.0.1"

/**
 * ============================================================================
 * 보드 타입 선택 (빌드시 -DBOARD_VERSION_NUM=1 또는 여기서 직접 설정)
 * ============================================================================
 */
#ifndef BOARD_VERSION_NUM
#define BOARD_VERSION_NUM 2  // 기본값: BASE_UBLOX
#endif

// 숫자로 선택 (주석 처리 방식보다 안전)
#if BOARD_VERSION_NUM == 1
  #define BOARD_TYPE_BASE_UNICORE
#elif BOARD_VERSION_NUM == 2
  #define BOARD_TYPE_BASE_UBLOX
#elif BOARD_VERSION_NUM == 3
  #define BOARD_TYPE_ROVER_UNICORE
#elif BOARD_VERSION_NUM == 4
  #define BOARD_TYPE_ROVER_UBLOX
#else
  #error "Invalid BOARD_VERSION_NUM! Must be 1-4"
#endif

// 컴파일 타임 체크
#if defined(BOARD_TYPE_BASE_UNICORE) + defined(BOARD_TYPE_BASE_UBLOX) + \
    defined(BOARD_TYPE_ROVER_UNICORE) + defined(BOARD_TYPE_ROVER_UBLOX) != 1
    #error "보드 타입을 하나만 설정해야 합니다"
#endif

#endif // BOARD_TYPE_H
