#ifndef BOARD_TYPE_H
#define BOARD_TYPE_H

/**
 * ============================================================
 * 현재 선택된 보드: PCB1 (F9P + BLE + LoRa)
 * ============================================================
 *
 * 이 파일은 자동 생성되었습니다.
 * 보드를 변경하려면: ./select_board.sh [보드타입]
 *
 * 또는 수동으로 이 파일을 편집할 수도 있습니다.
 */

// ====== 현재 선택된 보드 ======

#define BOARD_TYPE_PCB1
     // F9P + BLE + LoRa
// #define BOARD_TYPE_PCB2
     // UM982 + BLE + LoRa
// #define BOARD_TYPE_PCB3
     // F9P x2 + RS485 + LoRa
// #define BOARD_TYPE_PCB4
     // UM982 + RS485 + LoRa

// =================================

// 설정 검증
#if defined(BOARD_TYPE_PCB1) + defined(BOARD_TYPE_PCB2) + \
    defined(BOARD_TYPE_PCB3) + defined(BOARD_TYPE_PCB4) != 1
    #error "정확히 하나의 보드 타입만 선택해야 합니다!"
#endif

#endif /* BOARD_TYPE_H */
