/**
 * ============================================================
 * 보드 타입 선택
 * ============================================================
 *
 * 빌드하기 전에 아래에서 사용할 보드를 선택하세요.
 * 정확히 하나만 활성화해야 합니다!
 */

#ifndef BOARD_TYPE_H
#define BOARD_TYPE_H

/* ====== 아래 중 하나만 선택 (나머지는 주석 처리) ====== */

#define BOARD_TYPE_PCB1     // F9P + BLE + LoRa
// #define BOARD_TYPE_PCB2  // UM982 + BLE + LoRa
// #define BOARD_TYPE_PCB3  // F9P x2 + RS485 + LoRa
// #define BOARD_TYPE_PCB4  // UM982 + RS485 + LoRa

/* ======================================================== */

// 컴파일 타임 검증: 정확히 하나만 선택되었는지 확인
#if defined(BOARD_TYPE_PCB1) + defined(BOARD_TYPE_PCB2) + \
    defined(BOARD_TYPE_PCB3) + defined(BOARD_TYPE_PCB4) != 1
    #error "정확히 하나의 보드 타입만 선택해야 합니다!"
#endif

#endif /* BOARD_TYPE_H */
