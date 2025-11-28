# RAK4270 LoRa P2P 사용 가이드

RAK4270 모듈을 이용한 LoRa P2P 통신 라이브러리 사용법입니다.

## 목차
- [개요](#개요)
- [하드웨어 설정](#하드웨어-설정)
- [초기화](#초기화)
- [동기 명령어](#동기-명령어)
- [비동기 명령어](#비동기-명령어)
- [P2P 설정](#p2p-설정)
- [P2P 송신](#p2p-송신)
- [P2P 수신](#p2p-수신)
- [예제 코드](#예제-코드)

## 개요

이 라이브러리는 RAK4270 LoRa 모듈과 UART 통신을 통해 P2P 모드로 데이터를 송수신하는 기능을 제공합니다.

### 주요 기능
- ✅ 동기/비동기 AT 명령어 전송
- ✅ OK/ERROR 응답 자동 파싱
- ✅ P2P 모드 설정 및 구성
- ✅ P2P 데이터 송신
- ✅ P2P 데이터 수신 (콜백 방식)
- ✅ FreeRTOS 기반 Producer-Consumer 아키텍처

## 하드웨어 설정

### UART 핀 연결
- **USART3** 사용
- TX: PB10
- RX: PB11
- Baud rate: 115200 bps
- DMA 사용 (Circular mode)

### RAK4270 모듈 연결
```
STM32 USART3 TX (PB10) → RAK4270 RX
STM32 USART3 RX (PB11) → RAK4270 TX
GND → GND
3.3V → VCC
```

## 초기화

### 1. LoRa 인스턴스 생성 및 초기화

```c
#include "lora.h"
#include "usart.h"

// 전역 LoRa 인스턴스
static lora_t g_lora;

// 초기화
void app_lora_init(void) {
  // UART3 핸들을 전달하여 초기화
  lora_init(&g_lora, &huart3);

  // P2P 수신 콜백 설정
  lora_set_p2p_rx_callback(&g_lora, my_p2p_rx_callback);
}
```

### 2. UART 수신 데이터 연결

DMA 또는 UART 인터럽트에서 수신한 데이터를 LoRa 파서에 전달:

```c
// UART DMA 수신 완료 콜백
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART3) {
    // 수신 버퍼와 길이를 전달
    lora_uart_rx_process(&g_lora, rx_buffer, rx_length);
  }
}
```

## 동기 명령어

동기 명령어는 응답을 기다린 후 결과를 반환합니다.

### API

```c
bool lora_send_at_cmd_sync(lora_t *lora,
                           lora_cmd_t cmd,
                           const char *params,
                           char *response,
                           size_t response_size,
                           uint32_t timeout_ms);
```

### 예제

```c
char response[128];
bool result;

// 1. AT 명령어 테스트
result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_AT, NULL,
                               response, sizeof(response), 5000);
if (result) {
  printf("AT OK: %s\n", response);
} else {
  printf("AT ERROR\n");
}

// 2. 펌웨어 버전 확인
result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_GET_VERSION, NULL,
                               response, sizeof(response), 5000);
if (result) {
  printf("Version: %s\n", response);
}

// 3. 현재 모드 확인
result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_GET_MODE, NULL,
                               response, sizeof(response), 5000);
if (result) {
  printf("Current mode: %s\n", response);
}
```

### 결과 확인

- **result == true**: 명령어가 성공적으로 실행되고 "OK" 응답을 받음
- **result == false**: 명령어 실행 실패 또는 "ERROR" 응답 수신

## 비동기 명령어

비동기 명령어는 명령어를 전송한 후 즉시 반환하고, 응답은 콜백 함수로 전달됩니다.

### API

```c
bool lora_send_at_cmd_async(lora_t *lora,
                            lora_cmd_t cmd,
                            const char *params,
                            lora_at_cmd_handler callback,
                            uint32_t timeout_ms);
```

### 콜백 함수 정의

```c
// 콜백 함수 프로토타입
typedef void (*lora_at_cmd_handler)(lora_t *lora,
                                    lora_cmd_t cmd,
                                    void *msg,
                                    bool is_ok);

// 구현 예제
void my_async_callback(lora_t *lora, lora_cmd_t cmd, void *msg, bool is_ok) {
  printf("Command %d completed: %s\n", cmd, is_ok ? "OK" : "ERROR");

  if (is_ok) {
    printf("Success!\n");
  } else {
    printf("Failed!\n");
  }
}
```

### 예제

```c
// 비동기 AT 명령어 전송
bool result = lora_send_at_cmd_async(&g_lora, LORA_CMD_AT, NULL,
                                     my_async_callback, 5000);
if (result) {
  printf("Command sent successfully\n");
  // 콜백 함수가 호출될 때까지 기다리지 않고 다른 작업 수행 가능
} else {
  printf("Failed to send command\n");
}

// 버전 확인 (비동기)
result = lora_send_at_cmd_async(&g_lora, LORA_CMD_GET_VERSION, NULL,
                                my_async_callback, 5000);
```

### 동기 vs 비동기 비교

| 특징 | 동기 (Sync) | 비동기 (Async) |
|------|-------------|----------------|
| 응답 대기 | 응답을 받을 때까지 블로킹 | 즉시 반환 (논블로킹) |
| 결과 확인 | 반환값으로 즉시 확인 | 콜백 함수로 확인 |
| 사용 케이스 | 순차적 명령어 실행 | 병렬 작업, 이벤트 기반 |
| 세마포어 | 사용 (자동 생성/삭제) | 미사용 |

## P2P 설정

### 1. P2P 모드 설정

```c
// P2P 모드 활성화
bool result = lora_set_p2p_mode(&g_lora, true);
if (result) {
  printf("P2P mode enabled\n");
}

// LoRaWAN 모드로 전환
result = lora_set_p2p_mode(&g_lora, false);
```

### 2. P2P 파라미터 설정

```c
// P2P 설정 구조체
lora_p2p_config_t config = {
  .frequency = 922000000,        // 922 MHz (주파수)
  .spreading_factor = 7,         // SF7 (확산 계수)
  .bandwidth = 0,                // 0=125kHz, 1=250kHz, 2=500kHz
  .coding_rate = 1,              // 1=4/5, 2=4/6, 3=4/7, 4=4/8
  .preamble_length = 8,          // 프리앰블 길이 (2-65535)
  .tx_power = 14                 // 송신 전력 (5-22 dBm)
};

// 설정 적용
bool result = lora_configure_p2p(&g_lora, &config);
if (result) {
  printf("P2P configuration applied\n");
}
```

### 3. 설정 확인

```c
char response[128];
bool result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_GET_P2P_CONFIG, NULL,
                                    response, sizeof(response), 5000);
if (result) {
  printf("Current P2P config: %s\n", response);
}
```

## P2P 송신

### API

```c
bool lora_p2p_send(lora_t *lora, const uint8_t *data, size_t len);
```

### 예제

```c
// 문자열 전송
const char *message = "Hello LoRa!";
bool result = lora_p2p_send(&g_lora, (const uint8_t *)message, strlen(message));
if (result) {
  printf("Message sent successfully\n");
} else {
  printf("Send failed\n");
}

// 바이너리 데이터 전송
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
result = lora_p2p_send(&g_lora, data, sizeof(data));
```

### 제한사항
- 최대 256바이트까지 전송 가능

## P2P 수신

### 1. 수신 콜백 함수 정의

```c
// 수신 콜백 프로토타입
typedef void (*lora_p2p_rx_handler)(lora_t *lora, lora_p2p_rx_data_t *rx_data);

// 수신 데이터 구조체
typedef struct {
  int16_t rssi;              // RSSI 값 (dBm)
  int16_t snr;               // SNR 값 (dB)
  uint16_t len;              // 데이터 길이
  uint8_t data[256];         // 수신 데이터
} lora_p2p_rx_data_t;

// 구현 예제
void my_p2p_rx_callback(lora_t *lora, lora_p2p_rx_data_t *rx_data) {
  printf("\n=== P2P Data Received ===\n");
  printf("RSSI: %d dBm\n", rx_data->rssi);
  printf("SNR: %d dB\n", rx_data->snr);
  printf("Length: %u bytes\n", rx_data->len);

  // HEX 출력
  printf("Data (hex): ");
  for (uint16_t i = 0; i < rx_data->len; i++) {
    printf("%02X ", rx_data->data[i]);
  }
  printf("\n");

  // ASCII 출력
  printf("Data (ASCII): ");
  for (uint16_t i = 0; i < rx_data->len; i++) {
    if (rx_data->data[i] >= 32 && rx_data->data[i] < 127) {
      printf("%c", rx_data->data[i]);
    } else {
      printf(".");
    }
  }
  printf("\n========================\n");
}
```

### 2. 콜백 등록

```c
lora_set_p2p_rx_callback(&g_lora, my_p2p_rx_callback);
```

### 3. 수신 모드 시작

```c
// 연속 수신 모드 (timeout = 0)
bool result = lora_p2p_start_rx(&g_lora, 0);
if (result) {
  printf("Continuous receive mode started\n");
}

// 타임아웃 있는 수신 모드 (예: 30초)
result = lora_p2p_start_rx(&g_lora, 30000);
if (result) {
  printf("Receive mode started (30s timeout)\n");
}
```

### 4. 수신 모드 중지

```c
bool result = lora_p2p_stop_rx(&g_lora);
if (result) {
  printf("Receive mode stopped\n");
}
```

## 예제 코드

### 완전한 예제 1: 동기 명령어 테스트

```c
void test_sync_commands(void) {
  char response[128];
  bool result;

  // 1. 초기화
  lora_init(&g_lora, &huart3);

  // 2. AT 테스트
  result = lora_send_at_cmd_sync(&g_lora, LORA_CMD_AT, NULL,
                                 response, sizeof(response), 5000);
  printf("AT: %s (%s)\n", result ? "OK" : "ERROR", response);

  // 3. P2P 모드 설정
  result = lora_set_p2p_mode(&g_lora, true);
  printf("Set P2P mode: %s\n", result ? "OK" : "ERROR");

  // 4. P2P 설정
  lora_p2p_config_t config = {
    .frequency = 922000000,
    .spreading_factor = 7,
    .bandwidth = 0,
    .coding_rate = 1,
    .preamble_length = 8,
    .tx_power = 14
  };
  result = lora_configure_p2p(&g_lora, &config);
  printf("Configure P2P: %s\n", result ? "OK" : "ERROR");

  // 5. 데이터 전송
  const char *msg = "Test message";
  result = lora_p2p_send(&g_lora, (const uint8_t *)msg, strlen(msg));
  printf("Send: %s\n", result ? "OK" : "ERROR");
}
```

### 완전한 예제 2: 비동기 명령어 테스트

```c
void async_callback(lora_t *lora, lora_cmd_t cmd, void *msg, bool is_ok) {
  printf("Async cmd %d: %s\n", cmd, is_ok ? "OK" : "ERROR");
}

void test_async_commands(void) {
  bool result;

  // 1. 초기화
  lora_init(&g_lora, &huart3);

  // 2. 비동기 AT 명령어
  result = lora_send_at_cmd_async(&g_lora, LORA_CMD_AT, NULL,
                                  async_callback, 5000);
  printf("AT sent: %s\n", result ? "OK" : "Failed");

  // 3. 비동기 버전 확인
  result = lora_send_at_cmd_async(&g_lora, LORA_CMD_GET_VERSION, NULL,
                                  async_callback, 5000);
  printf("Version request sent: %s\n", result ? "OK" : "Failed");

  // 콜백이 호출될 때까지 대기하거나 다른 작업 수행
  vTaskDelay(pdMS_TO_TICKS(3000));
}
```

### 완전한 예제 3: P2P 송수신

```c
void p2p_rx_callback(lora_t *lora, lora_p2p_rx_data_t *rx_data) {
  printf("Received %u bytes, RSSI: %d dBm\n", rx_data->len, rx_data->rssi);
  printf("Data: ");
  for (uint16_t i = 0; i < rx_data->len; i++) {
    printf("%c", rx_data->data[i]);
  }
  printf("\n");
}

void test_p2p_communication(void) {
  // 초기화
  lora_init(&g_lora, &huart3);
  lora_set_p2p_rx_callback(&g_lora, p2p_rx_callback);

  // P2P 모드 설정
  lora_set_p2p_mode(&g_lora, true);

  // P2P 파라미터 설정
  lora_p2p_config_t config = {
    .frequency = 922000000,
    .spreading_factor = 7,
    .bandwidth = 0,
    .coding_rate = 1,
    .preamble_length = 8,
    .tx_power = 14
  };
  lora_configure_p2p(&g_lora, &config);

  // 수신 시작
  lora_p2p_start_rx(&g_lora, 0);  // 연속 수신

  // 데이터 전송 (다른 장치에서)
  const char *msg = "Hello from Device 1";
  lora_p2p_send(&g_lora, (const uint8_t *)msg, strlen(msg));
}
```

## 지원하는 AT 명령어

| 명령어 | 설명 | 예제 |
|--------|------|------|
| LORA_CMD_AT | AT 테스트 | `AT` |
| LORA_CMD_RESET | 모듈 리셋 | `AT+RESET` |
| LORA_CMD_SET_MODE | 모드 설정 | `AT+MODE=0` (P2P) |
| LORA_CMD_GET_MODE | 모드 확인 | `AT+MODE=?` |
| LORA_CMD_SET_P2P_CONFIG | P2P 설정 | `AT+P2P=922000000:7:0:1:8:14` |
| LORA_CMD_GET_P2P_CONFIG | P2P 설정 확인 | `AT+P2P=?` |
| LORA_CMD_P2P_SEND | P2P 전송 | `AT+PSEND=48656C6C6F` |
| LORA_CMD_P2P_RX_MODE | P2P 수신 시작 | `AT+PRECV=0` |
| LORA_CMD_P2P_STOP_RX | P2P 수신 중지 | `AT+PRECV=0` |
| LORA_CMD_GET_VERSION | 버전 확인 | `AT+VER=?` |

## 응답 형식

### OK 응답
```
OK\r\n
```

### ERROR 응답
```
ERROR\r\n
AT_ERROR\r\n
AT_PARAM_ERROR\r\n
```

### P2P 수신 이벤트
```
+EVT:RXP2P:-45:8:11:48656C6C6F20576F726C64\r\n
```
형식: `+EVT:RXP2P:RSSI:SNR:LEN:DATA(HEX)`

### TX 완료 이벤트
```
+EVT:TXP2P DONE\r\n
```

## 문제 해결

### 1. 명령어가 응답하지 않음
- UART 연결 확인 (TX/RX 교차 연결)
- Baud rate 확인 (115200 bps)
- 모듈 전원 확인

### 2. OK/ERROR가 파싱되지 않음
- `lora_uart_rx_process()` 함수가 UART 수신 콜백에서 호출되는지 확인
- 수신 버퍼 크기 확인

### 3. P2P 수신이 안됨
- P2P 설정이 송수신 양쪽에서 동일한지 확인
- 주파수, SF, BW, CR이 일치하는지 확인
- 수신 모드가 활성화되어 있는지 확인

### 4. 콜백이 호출되지 않음
- 콜백 함수가 등록되었는지 확인
- UART 데이터가 파서로 전달되는지 확인

## 참고 자료

- [RAK4270 AT Command Manual](https://docs.rakwireless.com/Product-Categories/WisDuo/RAK4270-Module/AT-Command-Manual/)
- [LoRa P2P 통신 기초](https://lora-developers.semtech.com/)

## 라이센스

이 라이브러리는 프로젝트 라이센스를 따릅니다.
