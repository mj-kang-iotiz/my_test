#!/bin/bash

# 보드 선택 스크립트
# 사용법: ./select_board.sh PCB1

BOARD_TYPE_FILE="config/board_type.h"

if [ $# -eq 0 ]; then
    echo "사용법: $0 [PCB1|PCB2|PCB3|PCB4]"
    echo ""
    echo "예시:"
    echo "  $0 PCB1  - F9P + BLE + LoRa"
    echo "  $0 PCB2  - UM982 + BLE + LoRa"
    echo "  $0 PCB3  - F9P x2 + RS485 + LoRa"
    echo "  $0 PCB4  - UM982 + RS485 + LoRa"
    exit 1
fi

SELECTED_BOARD=$1

case $SELECTED_BOARD in
    PCB1|pcb1)
        BOARD="PCB1"
        DESC="F9P + BLE + LoRa"
        ;;
    PCB2|pcb2)
        BOARD="PCB2"
        DESC="UM982 + BLE + LoRa"
        ;;
    PCB3|pcb3)
        BOARD="PCB3"
        DESC="F9P x2 + RS485 + LoRa"
        ;;
    PCB4|pcb4)
        BOARD="PCB4"
        DESC="UM982 + RS485 + LoRa"
        ;;
    *)
        echo "오류: 잘못된 보드 타입 '$SELECTED_BOARD'"
        echo "PCB1, PCB2, PCB3, PCB4 중 하나를 선택하세요."
        exit 1
        ;;
esac

# board_type.h 파일 생성
cat > $BOARD_TYPE_FILE << EOF
#ifndef BOARD_TYPE_H
#define BOARD_TYPE_H

/**
 * ============================================================
 * 현재 선택된 보드: ${BOARD} (${DESC})
 * ============================================================
 *
 * 이 파일은 자동 생성되었습니다.
 * 보드를 변경하려면: ./select_board.sh [보드타입]
 *
 * 또는 수동으로 이 파일을 편집할 수도 있습니다.
 */

// ====== 현재 선택된 보드 ======

EOF

# 각 보드 정의 추가
for B in PCB1 PCB2 PCB3 PCB4; do
    if [ "$B" = "$BOARD" ]; then
        echo "#define BOARD_TYPE_${B}" >> $BOARD_TYPE_FILE
    else
        echo "// #define BOARD_TYPE_${B}" >> $BOARD_TYPE_FILE
    fi

    # 설명 추가
    case $B in
        PCB1) echo "     // F9P + BLE + LoRa" >> $BOARD_TYPE_FILE ;;
        PCB2) echo "     // UM982 + BLE + LoRa" >> $BOARD_TYPE_FILE ;;
        PCB3) echo "     // F9P x2 + RS485 + LoRa" >> $BOARD_TYPE_FILE ;;
        PCB4) echo "     // UM982 + RS485 + LoRa" >> $BOARD_TYPE_FILE ;;
    esac
done

cat >> $BOARD_TYPE_FILE << 'EOF'

// =================================

// 설정 검증
#if defined(BOARD_TYPE_PCB1) + defined(BOARD_TYPE_PCB2) + \
    defined(BOARD_TYPE_PCB3) + defined(BOARD_TYPE_PCB4) != 1
    #error "정확히 하나의 보드 타입만 선택해야 합니다!"
#endif

#endif /* BOARD_TYPE_H */
EOF

echo "✅ 보드가 ${BOARD} (${DESC})로 설정되었습니다."
echo ""
echo "이제 프로젝트를 빌드하세요!"
echo ""
cat $BOARD_TYPE_FILE
