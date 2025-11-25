#ifndef GPS_BOARD_V1_H
#define GPS_BOARD_V1_H

#include "stm32f4xx_hal.h"

/**
 * @brief Board V1: Unicore Base
 * - GPS 1개 (Base)
 * - UART2 사용
 */

// GPS Base (Unicore) 설정
#define GPS_BASE_UART           USART2
#define GPS_BASE_UART_IRQ       USART2_IRQn
#define GPS_BASE_DMA            DMA1_Stream5
#define GPS_BASE_DMA_CHANNEL    DMA_CHANNEL_4

// GPS Base 핀 설정
#define GPS_BASE_TX_PIN         GPIO_PIN_2
#define GPS_BASE_TX_PORT        GPIOA
#define GPS_BASE_RX_PIN         GPIO_PIN_3
#define GPS_BASE_RX_PORT        GPIOA
#define GPS_BASE_RST_PIN        GPIO_PIN_5
#define GPS_BASE_RST_PORT       GPIOA
#define GPS_BASE_AF             GPIO_AF7_USART2

// Unicore 특화 설정
#define GPS_BASE_BAUDRATE       115200
#define GPS_BASE_PROTOCOL       GPS_PROTOCOL_NMEA  // Unicore는 주로 NMEA

#endif // GPS_BOARD_V1_H
