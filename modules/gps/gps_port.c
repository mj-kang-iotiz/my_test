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
#include "FreeRTOS.h"
#include "task.h"

extern char gps_recv[2048];

/* ============================================================
 * 보드별 핀맵 정의 (이 파일 내부에서만 사용)
 * ============================================================ */

#if defined(BOARD_TYPE_PCB1)
    // PCB1: F9P GPS + BLE + LoRa
    #define GPS1_UART                   USART2
    #define GPS1_TX_PIN                 LL_GPIO_PIN_2
    #define GPS1_RX_PIN                 LL_GPIO_PIN_3
    #define GPS1_GPIO_PORT              GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    #define GPS1_DMA_STREAM             LL_DMA_STREAM_5
    #define GPS1_DMA_CHANNEL            LL_DMA_CHANNEL_4

#elif defined(BOARD_TYPE_PCB2)
    // PCB2: UM982 GPS + BLE + LoRa
    #define GPS1_UART                   USART2
    #define GPS1_TX_PIN                 LL_GPIO_PIN_2
    #define GPS1_RX_PIN                 LL_GPIO_PIN_3
    #define GPS1_GPIO_PORT              GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    #define GPS1_DMA_STREAM             LL_DMA_STREAM_5
    #define GPS1_DMA_CHANNEL            LL_DMA_CHANNEL_4

#elif defined(BOARD_TYPE_PCB3)
    // PCB3: F9P x2 + RS485 + LoRa (듀얼 GPS)
    // GPS1 (Primary)
    #define GPS1_UART                   USART2
    #define GPS1_TX_PIN                 LL_GPIO_PIN_2
    #define GPS1_RX_PIN                 LL_GPIO_PIN_3
    #define GPS1_GPIO_PORT              GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    #define GPS1_DMA_STREAM             LL_DMA_STREAM_5
    #define GPS1_DMA_CHANNEL            LL_DMA_CHANNEL_4

    // GPS2 (Secondary) - 듀얼 GPS 지원 시 추가 구현 필요
    // #define GPS2_UART                USART3
    // ...

#elif defined(BOARD_TYPE_PCB4)
    // PCB4: UM982 + RS485 + LoRa
    #define GPS1_UART                   USART2
    #define GPS1_TX_PIN                 LL_GPIO_PIN_2
    #define GPS1_RX_PIN                 LL_GPIO_PIN_3
    #define GPS1_GPIO_PORT              GPIOA
    #define GPS1_RESET_PIN              GPIO_PIN_5
    #define GPS1_RESET_PORT             GPIOA
    #define GPS1_DMA_STREAM             LL_DMA_STREAM_5
    #define GPS1_DMA_CHANNEL            LL_DMA_CHANNEL_4

#endif

/* ============================================================
 * 내부 함수 선언
 * ============================================================ */

static void gps_dma_init(void);
static void gps_uart_init(void);
static void gps_gpio_init(void);
static bool gps_wait_for_um982_ready(uint32_t timeout_ms);

/* ============================================================
 * 하드웨어 초기화
 * ============================================================ */

/**
 * @brief DMA 초기화
 */
static void gps_dma_init(void) {
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  NVIC_SetPriority(DMA1_Stream5_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

/**
 * @brief UART 초기화
 */
static void gps_uart_init(void) {
  LL_USART_InitTypeDef USART_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);

  /* USART2 GPIO Configuration */
  GPIO_InitStruct.Pin = GPS1_TX_PIN | GPS1_RX_PIN;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPS1_GPIO_PORT, &GPIO_InitStruct);

  /* USART2 DMA Init */
  LL_DMA_SetChannelSelection(DMA1, GPS1_DMA_STREAM, GPS1_DMA_CHANNEL);
  LL_DMA_SetDataTransferDirection(DMA1, GPS1_DMA_STREAM,
                                  LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  LL_DMA_SetStreamPriorityLevel(DMA1, GPS1_DMA_STREAM, LL_DMA_PRIORITY_LOW);
  LL_DMA_SetMode(DMA1, GPS1_DMA_STREAM, LL_DMA_MODE_CIRCULAR);
  LL_DMA_SetPeriphIncMode(DMA1, GPS1_DMA_STREAM, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, GPS1_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, GPS1_DMA_STREAM, LL_DMA_PDATAALIGN_BYTE);
  LL_DMA_SetMemorySize(DMA1, GPS1_DMA_STREAM, LL_DMA_MDATAALIGN_BYTE);
  LL_DMA_DisableFifoMode(DMA1, GPS1_DMA_STREAM);

  /* USART2 interrupt Init */
  NVIC_SetPriority(USART2_IRQn,
                   NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
  NVIC_EnableIRQ(USART2_IRQn);

  /* USART Configuration */
  USART_InitStruct.BaudRate = 38400;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(GPS1_UART, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(GPS1_UART);
  LL_USART_Enable(GPS1_UART);
}

/**
 * @brief GPIO 초기화 (Reset 핀 등)
 */
static void gps_gpio_init(void) {
  // Reset 핀은 이미 gpio.c에서 초기화되어 있다고 가정
  // 필요시 여기서 초기화 추가
}

/**
 * @brief GPS 하드웨어 초기화 (보드 독립적)
 */
void gps_port_init(void) {
  gps_dma_init();
  gps_uart_init();
  gps_gpio_init();
}

/* ============================================================
 * GPS 타입별 초기화
 * ============================================================ */

/**
 * @brief F9P GPS 초기화
 * F9P는 Reset 후 즉시 사용 가능
 */
void gps_init_f9p(void) {
  // 1. Reset 핀 제어
  HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_SET);

  // 2. F9P는 즉시 사용 가능 - 별도 대기 불필요

  // 3. 필요시 UBX 설정 명령 전송
  // TODO: UBX configuration commands
}

/**
 * @brief UM982 GPS 초기화
 * UM982는 UART로 RDY 응답을 확인해야 함
 */
void gps_init_um982(void) {
  // 1. Reset 핀 토글
  HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_RESET);
  vTaskDelay(pdMS_TO_TICKS(100));
  HAL_GPIO_WritePin(GPS1_RESET_PORT, GPS1_RESET_PIN, GPIO_PIN_SET);

  // 2. UART로 RDY 응답 대기 (최대 5초)
  if (!gps_wait_for_um982_ready(5000)) {
    // RDY 응답 수신 실패
    // LOG_ERR("UM982 initialization timeout!");
    return;
  }

  // 3. 초기화 성공 - 설정 명령 전송
  // TODO: UM982 configuration commands
}

/**
 * @brief UM982 RDY 응답 대기 (UART 파싱)
 *
 * UM982는 부팅 후 UART로 "$command,response,*checksum\r\n" 형식의
 * 메시지를 보냅니다. 정확한 RDY 메시지 형식은 데이터시트 확인 필요.
 *
 * @param timeout_ms 타임아웃 (밀리초)
 * @return true: RDY 수신, false: 타임아웃
 */
static bool gps_wait_for_um982_ready(uint32_t timeout_ms) {
  uint32_t start_tick = xTaskGetTickCount();
  uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

  // TODO: 실제 구현 시 gps_recv 버퍼에서 RDY 메시지를 파싱
  // 예시: "$GNGGA" 같은 NMEA 메시지가 수신되면 준비 완료로 판단
  // 또는 UM982 전용 RDY 응답 메시지 확인

  while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
    // 버퍼에서 RDY 메시지 확인
    // if (strstr(gps_recv, "RDY") != NULL) {
    //     return true;
    // }

    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms마다 확인
  }

  return false;  // 타임아웃
}

/* ============================================================
 * GPS 통신 시작
 * ============================================================ */

/**
 * @brief GPS 통신 시작
 */
void gps_port_comm_start(void) {
  LL_DMA_SetPeriphAddress(DMA1, GPS1_DMA_STREAM, (uint32_t)&GPS1_UART->DR);
  LL_DMA_SetMemoryAddress(DMA1, GPS1_DMA_STREAM, (uint32_t)&gps_recv);
  LL_DMA_SetDataLength(DMA1, GPS1_DMA_STREAM, sizeof(gps_recv));

  LL_DMA_EnableIT_TE(DMA1, GPS1_DMA_STREAM);
  LL_DMA_EnableIT_FE(DMA1, GPS1_DMA_STREAM);
  LL_DMA_EnableIT_DME(DMA1, GPS1_DMA_STREAM);

  LL_USART_EnableIT_IDLE(GPS1_UART);
  LL_USART_EnableIT_PE(GPS1_UART);
  LL_USART_EnableIT_ERROR(GPS1_UART);
  LL_USART_EnableDMAReq_RX(GPS1_UART);

  LL_DMA_EnableStream(DMA1, GPS1_DMA_STREAM);
  LL_USART_Enable(GPS1_UART);
}

/**
 * @brief GPS GPIO 핀 동작 (Reset 등)
 */
void gps_port_gpio_start(void) {
  // GPS 타입에 따라 초기화 수행
  const board_config_t* config = board_get_config();

  if (config->gps_primary == GPS_TYPE_F9P) {
    gps_init_f9p();
  } else if (config->gps_primary == GPS_TYPE_UM982) {
    gps_init_um982();
  }
}

/**
 * @brief GPS 시작 (통신 + GPIO)
 */
void gps_start(void) {
  gps_port_comm_start();
  gps_port_gpio_start();
}

/**
 * @brief DMA 버퍼 위치 반환
 * @return 현재 수신된 바이트 수
 */
uint32_t gps_get_rx_pos(void) {
  return sizeof(gps_recv) - LL_DMA_GetDataLength(DMA1, GPS1_DMA_STREAM);
}

/* ============================================================
 * 인터럽트 핸들러
 * ============================================================ */

/**
 * @brief USART2 인터럽트 핸들러
 */
void USART2_IRQHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (LL_USART_IsActiveFlag_IDLE(GPS1_UART)) {
    uint8_t dummy = 0;
    xQueueSendFromISR(gps_queue, &dummy, &xHigherPriorityTaskWoken);
    LL_USART_ClearFlag_IDLE(GPS1_UART);
  }

  if (LL_USART_IsActiveFlag_PE(GPS1_UART)) {
    LL_USART_ClearFlag_PE(GPS1_UART);
  }
  if (LL_USART_IsActiveFlag_FE(GPS1_UART)) {
    LL_USART_ClearFlag_FE(GPS1_UART);
  }
  if (LL_USART_IsActiveFlag_ORE(GPS1_UART)) {
    LL_USART_ClearFlag_ORE(GPS1_UART);
  }
  if (LL_USART_IsActiveFlag_NE(GPS1_UART)) {
    LL_USART_ClearFlag_NE(GPS1_UART);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief DMA1 Stream5 인터럽트 핸들러
 */
void DMA1_Stream5_IRQHandler(void) {
  // DMA 에러 처리 필요시 여기서 구현
}
