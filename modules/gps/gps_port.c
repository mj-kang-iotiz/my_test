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

// ============================================================================
// GPS 인스턴스별 버퍼 및 큐
// ============================================================================
static char gps_recv_buf[GPS_ID_MAX][2048];
static QueueHandle_t gps_queues[GPS_ID_MAX] = {NULL};

// ============================================================================
// GPS 타입 및 ID 저장 (UART별)
// ============================================================================
static gps_type_t uart2_gps_type = GPS_TYPE_F9P;
static gps_type_t uart4_gps_type = GPS_TYPE_F9P;
static gps_id_t uart2_gps_id = GPS_ID_BASE;
static gps_id_t uart4_gps_id = GPS_ID_ROVER;

// ============================================================================
// GPS HAL ops 함수들
// ============================================================================

// USART2 초기화
static int gps_uart2_init(void) {
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  LL_USART_InitTypeDef USART_InitStruct = {0};

  // 클럭 활성화
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

  // GPIO 설정 (PA2: TX, PA3: RX)
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2 | LL_GPIO_PIN_3;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // DMA 설정
  __HAL_RCC_DMA1_CLK_ENABLE();
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

  NVIC_SetPriority(DMA1_Stream5_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(DMA1_Stream5_IRQn);

  // UART 인터럽트
  NVIC_SetPriority(USART2_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(USART2_IRQn);

  // UART 설정 (GPS 타입별 보드레이트)
  USART_InitStruct.BaudRate = (uart2_gps_type == GPS_TYPE_F9P) ? 115200 : 38400;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART2, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(USART2);
  LL_USART_Enable(USART2);

  LOG_INFO("USART2 초기화 완료 (보드레이트: %d)", USART_InitStruct.BaudRate);
  return 0;
}

// USART2용 송신 함수
static int gps_uart2_send(const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    while (!LL_USART_IsActiveFlag_TXE(USART2));
    LL_USART_TransmitData8(USART2, data[i]);
  }
  return len;
}

// USART2 시작
static int gps_uart2_start(void) {
  // DMA 설정
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_5, (uint32_t)&USART2->DR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_5, (uint32_t)gps_recv_buf[uart2_gps_id]);
  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_5, sizeof(gps_recv_buf[uart2_gps_id]));

  LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_5);
  LL_DMA_EnableIT_FE(DMA1, LL_DMA_STREAM_5);
  LL_DMA_EnableIT_DME(DMA1, LL_DMA_STREAM_5);

  LL_USART_EnableIT_IDLE(USART2);
  LL_USART_EnableIT_PE(USART2);
  LL_USART_EnableIT_ERROR(USART2);
  LL_USART_EnableDMAReq_RX(USART2);

  LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_5);
  LL_USART_Enable(USART2);

  // GPIO 시작 (RST 핀)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

  LOG_INFO("USART2 통신 시작");
  return 0;
}

// USART2 정지
static int gps_uart2_stop(void) {
  // UART 인터럽트 비활성화
  LL_USART_DisableIT_IDLE(USART2);
  LL_USART_DisableIT_PE(USART2);
  LL_USART_DisableIT_ERROR(USART2);
  LL_USART_DisableDMAReq_RX(USART2);

  // DMA 비활성화
  LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_5);
  LL_DMA_DisableIT_TE(DMA1, LL_DMA_STREAM_5);
  LL_DMA_DisableIT_FE(DMA1, LL_DMA_STREAM_5);
  LL_DMA_DisableIT_DME(DMA1, LL_DMA_STREAM_5);

  // GPIO 정지 (RST 핀)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  LOG_INFO("USART2 통신 정지");
  return 0;
}

static const gps_hal_ops_t gps_uart2_ops = {
  .init = gps_uart2_init,
  .start = gps_uart2_start,
  .stop = gps_uart2_stop,
  .reset = NULL,
  .send = gps_uart2_send,
  .recv = NULL,
};

// UART4 초기화
static int gps_uart4_init(void) {
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  LL_USART_InitTypeDef USART_InitStruct = {0};

  // 클럭 활성화
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

  // GPIO 설정 (PA0: TX, PA1: RX)
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // DMA 설정
  __HAL_RCC_DMA1_CLK_ENABLE();
  LL_DMA_SetChannelSelection(DMA1, LL_DMA_STREAM_2, LL_DMA_CHANNEL_4);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_2,
                                   LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_2, LL_DMA_PRIORITY_LOW);
  LL_DMA_SetMode(DMA1, LL_DMA_STREAM_2, LL_DMA_MODE_CIRCULAR);
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_2, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_2, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_2, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_2, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_DisableFifoMode(DMA1, LL_DMA_STREAM_2);

  NVIC_SetPriority(DMA1_Stream2_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(DMA1_Stream2_IRQn);

  NVIC_SetPriority(UART4_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(UART4_IRQn);

  // UART 설정
  USART_InitStruct.BaudRate = (uart4_gps_type == GPS_TYPE_F9P) ? 115200 : 38400;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(UART4, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(UART4);
  LL_USART_Enable(UART4);

  LOG_INFO("UART4 초기화 완료 (보드레이트: %d)", USART_InitStruct.BaudRate);
  return 0;
}

// UART4용 송신 함수
static int gps_uart4_send(const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    while (!LL_USART_IsActiveFlag_TXE(UART4));
    LL_USART_TransmitData8(UART4, data[i]);
  }
  return len;
}

// UART4 시작
static int gps_uart4_start(void) {
  // DMA 설정
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_2, (uint32_t)&UART4->DR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_2, (uint32_t)gps_recv_buf[uart4_gps_id]);
  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_2, sizeof(gps_recv_buf[uart4_gps_id]));

  LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_2);
  LL_DMA_EnableIT_FE(DMA1, LL_DMA_STREAM_2);
  LL_DMA_EnableIT_DME(DMA1, LL_DMA_STREAM_2);

  LL_USART_EnableIT_IDLE(UART4);
  LL_USART_EnableIT_PE(UART4);
  LL_USART_EnableIT_ERROR(UART4);
  LL_USART_EnableDMAReq_RX(UART4);

  LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_2);
  LL_USART_Enable(UART4);

  // GPIO 시작 (RST 핀)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

  LOG_INFO("UART4 통신 시작");
  return 0;
}

// UART4 정지
static int gps_uart4_stop(void) {
  // UART 인터럽트 비활성화
  LL_USART_DisableIT_IDLE(UART4);
  LL_USART_DisableIT_PE(UART4);
  LL_USART_DisableIT_ERROR(UART4);
  LL_USART_DisableDMAReq_RX(UART4);

  // DMA 비활성화
  LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_2);
  LL_DMA_DisableIT_TE(DMA1, LL_DMA_STREAM_2);
  LL_DMA_DisableIT_FE(DMA1, LL_DMA_STREAM_2);
  LL_DMA_DisableIT_DME(DMA1, LL_DMA_STREAM_2);

  // GPIO 정지 (RST 핀)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  LOG_INFO("UART4 통신 정지");
  return 0;
}

static const gps_hal_ops_t gps_uart4_ops = {
  .init = gps_uart4_init,
  .start = gps_uart4_start,
  .stop = gps_uart4_stop,
  .reset = NULL,
  .send = gps_uart4_send,
  .recv = NULL,
};

// ============================================================================
// ✅ 핵심 함수: GPS 인스턴스 초기화 (board_config 기반)
// ============================================================================
int gps_port_init_instance(gps_t* gps_handle, gps_id_t id, gps_type_t type) {
  if (id >= GPS_ID_MAX) return -1;

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
    gps_handle->ops = &gps_uart2_ops;
    if (gps_handle->ops->init) {
      gps_handle->ops->init();
    }
    LOG_INFO("GPS[%d] USART2 할당 및 초기화 완료", id);

  } else if (config->board == BOARD_TYPE_ROVER_UM982) {
    // Rover Unicore: UART4
    uart4_gps_type = type;
    uart4_gps_id = id;
    gps_handle->ops = &gps_uart4_ops;
    if (gps_handle->ops->init) {
      gps_handle->ops->init();
    }
    LOG_INFO("GPS[%d] UART4 할당 및 초기화 완료", id);

  } else if (config->board == BOARD_TYPE_ROVER_F9P) {
    // Rover Ublox: 첫 번째는 USART2, 두 번째는 UART4
    if (id == GPS_ID_BASE) {
      uart2_gps_type = type;
      uart2_gps_id = id;
      gps_handle->ops = &gps_uart2_ops;
      if (gps_handle->ops->init) {
        gps_handle->ops->init();
      }
      LOG_INFO("GPS[%d] USART2 할당 및 초기화 완료 (Rover F9P 첫 번째)", id);
    } else if (id == GPS_ID_ROVER) {
      uart4_gps_type = type;
      uart4_gps_id = id;
      gps_handle->ops = &gps_uart4_ops;
      if (gps_handle->ops->init) {
        gps_handle->ops->init();
      }
      LOG_INFO("GPS[%d] UART4 할당 및 초기화 완료 (Rover F9P 두 번째)", id);
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
  if (id >= GPS_ID_MAX) return 0;

  const board_config_t* config = board_get_config();
  uint32_t dma_stream_num = 0;

  if (config->board == BOARD_TYPE_BASE_UM982 ||
      config->board == BOARD_TYPE_BASE_F9P ||
      (config->board == BOARD_TYPE_ROVER_F9P && id == GPS_ID_BASE)) {
    dma_stream_num = LL_DMA_STREAM_5;
  } else {
    dma_stream_num = LL_DMA_STREAM_2;
  }

  return sizeof(gps_recv_buf[id]) - LL_DMA_GetDataLength(DMA1, dma_stream_num);
}

/**
 * @brief GPS 수신 버퍼 포인터 가져오기
 */
char* gps_port_get_recv_buf(gps_id_t id) {
  if (id >= GPS_ID_MAX) return NULL;
  return gps_recv_buf[id];
}

/**
 * @brief GPS 인터럽트 핸들러용 큐 설정
 */
void gps_port_set_queue(gps_id_t id, QueueHandle_t queue) {
  if (id < GPS_ID_MAX) {
    gps_queues[id] = queue;
  }
}

// ============================================================================
// 인터럽트 핸들러
// ============================================================================

/**
 * @brief USART2 인터럽트 핸들러
 */
void USART2_IRQHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (LL_USART_IsActiveFlag_IDLE(USART2)) {
    uint8_t dummy = 0;

    // Base 또는 Rover F9P 첫 번째
    const board_config_t* config = board_get_config();
    gps_id_t id = GPS_ID_BASE;

    if (gps_queues[id]) {
      xQueueSendFromISR(gps_queues[id], &dummy, &xHigherPriorityTaskWoken);
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
 * @brief UART4 인터럽트 핸들러
 */
void UART4_IRQHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (LL_USART_IsActiveFlag_IDLE(UART4)) {
    uint8_t dummy = 0;

    // Rover Unicore 또는 Rover F9P 두 번째
    const board_config_t* config = board_get_config();
    gps_id_t id = (config->board == BOARD_TYPE_ROVER_F9P) ? GPS_ID_ROVER : GPS_ID_BASE;

    if (gps_queues[id]) {
      xQueueSendFromISR(gps_queues[id], &dummy, &xHigherPriorityTaskWoken);
    }

    LL_USART_ClearFlag_IDLE(UART4);
  }

  if (LL_USART_IsActiveFlag_PE(UART4)) {
    LL_USART_ClearFlag_PE(UART4);
  }
  if (LL_USART_IsActiveFlag_FE(UART4)) {
    LL_USART_ClearFlag_FE(UART4);
  }
  if (LL_USART_IsActiveFlag_ORE(UART4)) {
    LL_USART_ClearFlag_ORE(UART4);
  }
  if (LL_USART_IsActiveFlag_NE(UART4)) {
    LL_USART_ClearFlag_NE(UART4);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief DMA1 Stream5 인터럽트 핸들러 (USART2 RX)
 */
void DMA1_Stream5_IRQHandler(void) {
  // DMA 에러 처리
}

/**
 * @brief DMA1 Stream2 인터럽트 핸들러 (UART4 RX)
 */
void DMA1_Stream2_IRQHandler(void) {
  // DMA 에러 처리
}
