#ifndef GPS_PORT_H
#define GPS_PORT_H

#include "FreeRTOS.h"
#include "gps_app.h"
#include "queue.h"
#include "task.h"

void gps_port_init(void);
void gps_port_comm_start(void);
void gps_port_gpio_start(void);
void gps_start(void);
uint32_t gps_get_rx_pos(void);

void DMA1_Stream5_IRQHandler(void);
void USART2_IRQHandler(void);

#endif
