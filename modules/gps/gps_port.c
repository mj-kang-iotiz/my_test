#include "gps_port.h"
#include "board_config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_utils.h"

#ifndef TAG
  #define TAG "GPS_PORT"
#endif

#include "log.h"

static char gps_recv_buf[GPS_CNT][2048];
static QueueHandle_t gps_queues[GPS_CNT] = {NULL};

// UART TX mutex for thread-safe transmission
static SemaphoreHandle_t gps_uart_tx_mutex = NULL;

static gps_type_t uart2_gps_type = GPS_TYPE_F9P;
//static gps_type_t uart4_gps_type = GPS_TYPE_F9P;
static gps_id_t uart2_gps_id = GPS_ID_BASE;
//static gps_id_t uart4_gps_id = GPS_ID_ROVER;

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void gps_uart2_init(void) {
  /* USER CODE BEGIN USART2_Init 0 */
  /* USER CODE END USART2_Init 0 */

  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  /**USART2 GPIO Configuration
  PA2   ------> USART2_TX
  PA3   ------> USART2_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2 | LL_GPIO_PIN_3;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USART2 DMA Init */

  /* USART2_RX Init */
  LL_DMA_SetChannelSelection(DMA1, LL_DMA_STREAM_5, LL_DMA_CHANNEL_4);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_5,
                                  LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_5, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_STREAM_5, LL_DMA_MODE_CIRCULAR);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_5, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_5, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_5, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_5, LL_DMA_MDATAALIGN_BYTE);

  LL_DMA_DisableFifoMode(DMA1, LL_DMA_STREAM_5);

  /* USART2 interrupt Init */
  NVIC_SetPriority(USART2_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(USART2_IRQn);

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  USART_InitStruct.BaudRate = 38400;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART2, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(USART2);
  LL_USART_Enable(USART2);
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void gps_uart2_dma_init(void) {

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Stream5_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

/**
 * @brief GPS 하드웨어 초기화
 *
 */
int gps_rtk_uart2_init(void) {
  gps_uart2_dma_init();
  gps_uart2_init();

  return 0;
}

/**
 * @brief GPS 통신 시작
 *
 */
void gps_uart2_comm_start(void) {
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_5, (uint32_t)&USART2->DR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_5, (uint32_t)&gps_recv_buf[uart2_gps_id]);
  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_5, sizeof(gps_recv_buf[uart2_gps_id]));
  //  LL_DMA_EnableIT_HT(DMA2, LL_DMA_STREAM_1);
  //  LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_1);
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_5);
  LL_DMA_EnableIT_FE(DMA1, LL_DMA_STREAM_5);
  LL_DMA_EnableIT_DME(DMA1, LL_DMA_STREAM_5);

  LL_USART_EnableIT_IDLE(USART2);
  LL_USART_EnableIT_PE(USART2);
  LL_USART_EnableIT_ERROR(USART2);
  LL_USART_EnableDMAReq_RX(USART2);

  LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_5);
  LL_USART_Enable(USART2);
}

/**
 * @brief GPS GPIO 핀 동작
 *
 */
void gps_rtk_gpio_start(void) {
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // RTK Reset pin
//  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); // RTK WAKEUP pin
}

int gps_rtk_reset(void)
{
   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
   HAL_Delay(500);
   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

   return 0;
}

/**
 * @brief GPS enable
 *
 */
int gps_rtk_start(void) {
  gps_uart2_comm_start();
  gps_rtk_gpio_start();

  return 0;
}

int gps_uart2_send(const char *data, size_t len) {
  // Take mutex to prevent concurrent UART transmissions
  if (gps_uart_tx_mutex) {
    xSemaphoreTake(gps_uart_tx_mutex, portMAX_DELAY);
  }

  for (int i = 0; i < len; i++) {
    while (!LL_USART_IsActiveFlag_TXE(USART2))
      ;
    LL_USART_TransmitData8(USART2, *(data + i));
  }

  while (!LL_USART_IsActiveFlag_TC(USART2))
    ;

  // Release mutex
  if (gps_uart_tx_mutex) {
    xSemaphoreGive(gps_uart_tx_mutex);
  }

  return 0;
}

static const gps_hal_ops_t gps_rtk_uart2_ops = {
  .init = gps_rtk_uart2_init,
  .reset = gps_rtk_reset,
  .start = gps_rtk_start,
  .stop = NULL,
  .send = gps_uart2_send,
  .recv = NULL,
};

/**
 * @brief This function handles USART2 global interrupt.
 */
void USART2_IRQHandler(void) {
   BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (LL_USART_IsActiveFlag_IDLE(USART2)) {
    uint8_t dummy = 0;

    if (gps_queues[0]) {
      xQueueSendFromISR(gps_queues[0], &dummy, &xHigherPriorityTaskWoken);
    }

    LL_USART_ClearFlag_IDLE(USART2);
  }

  if (LL_USART_IsActiveFlag_PE(USART2)) {
    LL_USART_ClearFlag_PE(USART2);
  }
  if (LL_USART_IsActiveFlag_FE(USART2)) {
    LL_USART_ClearFlag_FE(USART2);
  }
  if (LL_USART_IsActiveFlag_ORE(USART2)) {
    LL_USART_ClearFlag_ORE(USART2);
  }
  if (LL_USART_IsActiveFlag_NE(USART2)) {
    LL_USART_ClearFlag_NE(USART2);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief This function handles DMA1 stream5 global interrupt.
 */
void DMA1_Stream5_IRQHandler(void) {}



int gps_port_init_instance(gps_t* gps_handle, gps_id_t id, gps_type_t type) {
  if (id >= GPS_ID_MAX) return -1;

  // Create UART TX mutex (once)
  if (gps_uart_tx_mutex == NULL) {
    gps_uart_tx_mutex = xSemaphoreCreateMutex();
  }

  const board_config_t* config = board_get_config();

  LOG_INFO("GPS[%d] Port 초기화 시작 (보드: %d, GPS 타입: %s)",
           id,
           config->board,
           type == GPS_TYPE_F9P ? "F9P" : "UM982");

  // ✅ 보드 타입과 GPS ID에 따라 적절한 UART 선택
  if (config->board == BOARD_TYPE_BASE_UM982 ||
      config->board == BOARD_TYPE_BASE_F9P) {
    // Base 보드: 항상 USART2
    uart2_gps_type = type;
    uart2_gps_id = id;
    gps_handle->ops = &gps_rtk_uart2_ops;
    if (gps_handle->ops->init) {
      gps_handle->ops->init();
    }
    LOG_INFO("GPS[%d] USART2 할당 및 초기화 완료", id);

  } else if (config->board == BOARD_TYPE_ROVER_UM982) {
//    // Rover Unicore: UART4
//    uart4_gps_type = type;
//    uart4_gps_id = id;
//    gps_handle->ops = &gps_uart4_ops;
//    if (gps_handle->ops->init) {
//      gps_handle->ops->init();
//    }
//    LOG_INFO("GPS[%d] UART4 할당 및 초기화 완료", id);

  } else if (config->board == BOARD_TYPE_ROVER_F9P) {
    // Rover Ublox: 첫 번째는 USART2, 두 번째는 UART4
    if (id == GPS_ID_BASE) {
      uart2_gps_type = type;
      uart2_gps_id = id;
      gps_handle->ops = &gps_rtk_uart2_ops;
      if (gps_handle->ops->init) {
        gps_handle->ops->init();
      }
      LOG_INFO("GPS[%d] USART2 할당 및 초기화 완료 (Rover F9P 첫 번째)", id);
    } else if (id == GPS_ID_ROVER) {
//      uart4_gps_type = type;
//      uart4_gps_id = id;
//      gps_handle->ops = &gps_uart4_ops;
//      if (gps_handle->ops->init) {
//        gps_handle->ops->init();
//      }
//      LOG_INFO("GPS[%d] UART4 할당 및 초기화 완료 (Rover F9P 두 번째)", id);
    }
  }

  return 0;
}

/**
 * @brief GPS 통신 시작
 */
void gps_port_start(gps_t* gps_handle) {
  if (!gps_handle || !gps_handle->ops || !gps_handle->ops->start) {
    LOG_ERR("GPS start failed: invalid handle or ops");
    return;
  }

  gps_handle->ops->start();
}

/**
 * @brief GPS 통신 정지
 */
void gps_port_stop(gps_t* gps_handle) {
  if (!gps_handle || !gps_handle->ops || !gps_handle->ops->stop) {
    LOG_ERR("GPS stop failed: invalid handle or ops");
    return;
  }

  gps_handle->ops->stop();
}

/**
 * @brief GPS 수신 버퍼 위치 가져오기
 */
uint32_t gps_port_get_rx_pos(gps_id_t id) {
  if (id >= GPS_CNT) return 0;

  const board_config_t* config = board_get_config();
  uint32_t dma_stream_num = 0;

  dma_stream_num = LL_DMA_STREAM_5;

  return sizeof(gps_recv_buf[id]) - LL_DMA_GetDataLength(DMA1, dma_stream_num);
}

/**
 * @brief GPS 수신 버퍼 포인터 가져오기
 */
char* gps_port_get_recv_buf(gps_id_t id) {
  if (id >= GPS_CNT) return NULL;
  return gps_recv_buf[id];
}

/**
 * @brief GPS 인터럽트 핸들러용 큐 설정
 */
void gps_port_set_queue(gps_id_t id, QueueHandle_t queue) {
  if (id < GPS_CNT) {
    gps_queues[id] = queue;
  }
}
