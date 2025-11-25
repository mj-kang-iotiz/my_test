#ifndef GPS_BOARD_V4_H
#define GPS_BOARD_V4_H

#include "stm32f4xx_hal.h"

/**
 * @brief Board V4: Ublox Rover (Dual F9P)
 * - GPS 2개 (Rover용 F9P 2개)
 * - UART2 (첫 번째 F9P) + UART4 (두 번째 F9P)
 */

// GPS Rover 첫 번째 (Ublox F9P) 설정
#define GPS_ROVER_UART          USART2
#define GPS_ROVER_UART_IRQ      USART2_IRQn
#define GPS_ROVER_DMA           DMA1_Stream5
#define GPS_ROVER_DMA_CHANNEL   DMA_CHANNEL_4

// GPS Rover 첫 번째 핀 설정
#define GPS_ROVER_TX_PIN        GPIO_PIN_2
#define GPS_ROVER_TX_PORT       GPIOA
#define GPS_ROVER_RX_PIN        GPIO_PIN_3
#define GPS_ROVER_RX_PORT       GPIOA
#define GPS_ROVER_RST_PIN       GPIO_PIN_5
#define GPS_ROVER_RST_PORT      GPIOA
#define GPS_ROVER_AF            GPIO_AF7_USART2

// Ublox F9P 특화 설정
#define GPS_ROVER_BAUDRATE      115200
#define GPS_ROVER_PROTOCOL      GPS_PROTOCOL_UBX

// ✅ GPS Rover 두 번째 (Ublox F9P) 설정
#define GPS_ROVER2_UART         UART4
#define GPS_ROVER2_UART_IRQ     UART4_IRQn
#define GPS_ROVER2_DMA          DMA1_Stream2
#define GPS_ROVER2_DMA_CHANNEL  DMA_CHANNEL_4

// GPS Rover 두 번째 핀 설정
#define GPS_ROVER2_TX_PIN       GPIO_PIN_0
#define GPS_ROVER2_TX_PORT      GPIOA
#define GPS_ROVER2_RX_PIN       GPIO_PIN_1
#define GPS_ROVER2_RX_PORT      GPIOA
#define GPS_ROVER2_RST_PIN      GPIO_PIN_8
#define GPS_ROVER2_RST_PORT     GPIOA
#define GPS_ROVER2_AF           GPIO_AF8_UART4

// Ublox F9P 특화 설정
#define GPS_ROVER2_BAUDRATE     115200
#define GPS_ROVER2_PROTOCOL     GPS_PROTOCOL_UBX

#endif // GPS_BOARD_V4_H
