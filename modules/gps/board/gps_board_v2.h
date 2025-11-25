#ifndef GPS_BOARD_V2_H
#define GPS_BOARD_V2_H

#include "stm32f4xx_hal.h"

/**
 * @brief Board V2: Unicore Rover
 * - GPS 1개 (Rover)
 * - UART4 사용 (Base와 다른 UART)
 */

// GPS Rover (Unicore) 설정
#define GPS_ROVER_UART          UART4
#define GPS_ROVER_UART_IRQ      UART4_IRQn
#define GPS_ROVER_DMA           DMA1_Stream2
#define GPS_ROVER_DMA_CHANNEL   DMA_CHANNEL_4

// GPS Rover 핀 설정
#define GPS_ROVER_TX_PIN        GPIO_PIN_0
#define GPS_ROVER_TX_PORT       GPIOA
#define GPS_ROVER_RX_PIN        GPIO_PIN_1
#define GPS_ROVER_RX_PORT       GPIOA
#define GPS_ROVER_RST_PIN       GPIO_PIN_8
#define GPS_ROVER_RST_PORT      GPIOA
#define GPS_ROVER_AF            GPIO_AF8_UART4

// Unicore 특화 설정
#define GPS_ROVER_BAUDRATE      115200
#define GPS_ROVER_PROTOCOL      GPS_PROTOCOL_NMEA

#endif // GPS_BOARD_V2_H
