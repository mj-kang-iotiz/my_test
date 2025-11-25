/**
 * ============================================================
 * 보드별 핀 매핑 정의
 * ============================================================
 *
 * 각 PCB 버전마다 다른 핀 배치를 정의합니다.
 */

#ifndef BOARD_PINMAP_H
#define BOARD_PINMAP_H

#include "board_type.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * GPS 핀 매핑
 * ============================================================ */

#if defined(BOARD_TYPE_PCB1)
    // PCB1: F9P GPS + BLE + LoRa
    #define GPS1_UART                   USART2
    #define GPS1_UART_TX_PIN            GPIO_PIN_2
    #define GPS1_UART_TX_PORT           GPIOA
    #define GPS1_UART_RX_PIN            GPIO_PIN_3
    #define GPS1_UART_RX_PORT           GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    // F9P에는 RDY 핀 없음

#elif defined(BOARD_TYPE_PCB2)
    // PCB2: UM982 GPS + BLE + LoRa
    #define GPS1_UART                   USART2
    #define GPS1_UART_TX_PIN            GPIO_PIN_2
    #define GPS1_UART_TX_PORT           GPIOA
    #define GPS1_UART_RX_PIN            GPIO_PIN_3
    #define GPS1_UART_RX_PORT           GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    // UM982 RDY 핀
    #define GPS1_RDY_PIN                GPIO_PIN_6
    #define GPS1_RDY_PORT               GPIOA

#elif defined(BOARD_TYPE_PCB3)
    // PCB3: F9P x2 + RS485 + LoRa (듀얼 GPS)
    // GPS 1 (Primary)
    #define GPS1_UART                   USART2
    #define GPS1_UART_TX_PIN            GPIO_PIN_2
    #define GPS1_UART_TX_PORT           GPIOA
    #define GPS1_UART_RX_PIN            GPIO_PIN_3
    #define GPS1_UART_RX_PORT           GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA

    // GPS 2 (Secondary)
    #define GPS2_UART                   USART3
    #define GPS2_UART_TX_PIN            GPIO_PIN_10
    #define GPS2_UART_TX_PORT           GPIOB
    #define GPS2_UART_RX_PIN            GPIO_PIN_11
    #define GPS2_UART_RX_PORT           GPIOB
    #define GPS2_RESET_PIN              GPIO_PIN_12
    #define GPS2_RESET_PORT             GPIOB

#elif defined(BOARD_TYPE_PCB4)
    // PCB4: UM982 + RS485 + LoRa
    #define GPS1_UART                   USART2
    #define GPS1_UART_TX_PIN            GPIO_PIN_2
    #define GPS1_UART_TX_PORT           GPIOA
    #define GPS1_UART_RX_PIN            GPIO_PIN_3
    #define GPS1_UART_RX_PORT           GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    // UM982 RDY 핀
    #define GPS1_RDY_PIN                GPIO_PIN_6
    #define GPS1_RDY_PORT               GPIOA

#endif

/* ============================================================
 * BLE 핀 매핑 (PCB1, PCB2만 사용)
 * ============================================================ */

#if defined(BOARD_TYPE_PCB1) || defined(BOARD_TYPE_PCB2)
    #define BLE_UART                    UART4
    #define BLE_UART_TX_PIN             GPIO_PIN_0
    #define BLE_UART_TX_PORT            GPIOA
    #define BLE_UART_RX_PIN             GPIO_PIN_1
    #define BLE_UART_RX_PORT            GPIOA
    #define BLE_RESET_PIN               GPIO_PIN_8
    #define BLE_RESET_PORT              GPIOA
#endif

/* ============================================================
 * RS485 핀 매핑 (PCB3, PCB4만 사용)
 * ============================================================ */

#if defined(BOARD_TYPE_PCB3) || defined(BOARD_TYPE_PCB4)
    #define RS485_UART                  UART5
    #define RS485_UART_TX_PIN           GPIO_PIN_12
    #define RS485_UART_TX_PORT          GPIOC
    #define RS485_UART_RX_PIN           GPIO_PIN_2
    #define RS485_UART_RX_PORT          GPIOD
    #define RS485_DE_PIN                GPIO_PIN_13  // Driver Enable
    #define RS485_DE_PORT               GPIOC
    #define RS485_RE_PIN                GPIO_PIN_14  // Receiver Enable
    #define RS485_RE_PORT               GPIOC
#endif

/* ============================================================
 * LoRa 핀 매핑 (모든 보드 공통)
 * ============================================================ */

#define LORA_UART                       USART1
#define LORA_UART_TX_PIN                GPIO_PIN_9
#define LORA_UART_TX_PORT               GPIOA
#define LORA_UART_RX_PIN                GPIO_PIN_10
#define LORA_UART_RX_PORT               GPIOA
#define LORA_RESET_PIN                  GPIO_PIN_11
#define LORA_RESET_PORT                 GPIOA

/* ============================================================
 * LTE (EC25) 핀 매핑 (모든 보드 공통)
 * ============================================================ */

#define LTE_UART                        USART6
#define LTE_UART_TX_PIN                 GPIO_PIN_6
#define LTE_UART_TX_PORT                GPIOC
#define LTE_UART_RX_PIN                 GPIO_PIN_7
#define LTE_UART_RX_PORT                GPIOC
#define LTE_PWRKEY_PIN                  GPIO_PIN_8
#define LTE_PWRKEY_PORT                 GPIOC
#define LTE_RESET_PIN                   GPIO_PIN_9
#define LTE_RESET_PORT                  GPIOC

/* ============================================================
 * LED 핀 매핑 (모든 보드 공통)
 * ============================================================ */

// LED는 기존 led.c/h에 정의되어 있다고 가정

#endif /* BOARD_PINMAP_H */
