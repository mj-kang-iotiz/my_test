# LoRa-to-GPS Bridge 사용 가이드

## 개요

LoRa P2P로 수신한 데이터를 GPS 모듈로 안전하게 전달하는 브릿지 시스템입니다.

### 문제점과 해결 방안

#### 문제 1: GPS UART 송신 충돌
**문제**: LoRa 수신 데이터를 GPS로 전송할 때, GPS 모듈이 자체 명령을 송신 중이면 데이터가 충돌할 수 있음

**해결**:
- GPS UART 송신 함수(`gps_uart2_send`)에 **뮤텍스 추가**
- 모든 UART 송신이 상호 배타적으로 실행되도록 보장
- LoRa → GPS 전송과 GPS 명령 전송이 안전하게 순차 실행

```c
// modules/gps/gps_port.c
int gps_uart2_send(const char *data, size_t len) {
  // 뮤텍스로 UART 송신 보호
  if (gps_uart_tx_mutex) {
    xSemaphoreTake(gps_uart_tx_mutex, portMAX_DELAY);
  }

  // UART 송신
  for (int i = 0; i < len; i++) {
    while (!LL_USART_IsActiveFlag_TXE(USART2))
      ;
    LL_USART_TransmitData8(USART2, *(data + i));
  }

  // 뮤텍스 해제
  if (gps_uart_tx_mutex) {
    xSemaphoreGive(gps_uart_tx_mutex);
  }

  return 0;
}
```

#### 문제 2: LoRa 수신 콜백 블로킹
**문제**: 매초 1KB의 데이터를 LoRa 수신 콜백에서 직접 GPS로 전송하면 콜백이 오래 블로킹되어 다음 수신을 방해할 수 있음

**해결**:
- **별도의 전용 태스크** 생성 (우선순위: `tskIDLE_PRIORITY + 3`)
- **큐(Queue)** 사용: LoRa 콜백은 데이터를 큐에 넣고 즉시 반환
- 전용 태스크가 큐에서 데이터를 꺼내 GPS로 전송
- LoRa 수신 콜백은 빠르게 반환되어 다음 수신 준비

```
[LoRa RX] → [콜백: 큐 전송] → [브릿지 태스크: GPS 전송]
   빠름!         빠름!              별도 태스크에서
```

#### 문제 3: 메모리 할당 오버헤드
**문제**: 매초 1KB씩 동적 할당하면 메모리 파편화와 할당 실패 위험

**해결**:
- **버퍼 풀(Buffer Pool)** 사용
- 10개의 1KB 버퍼를 미리 할당
- 사용 후 반환하여 재사용
- 동적 할당 없이 안정적 운영

## 아키텍처

```
┌─────────────────┐
│   LoRa Module   │
│   (RAK4270)     │
└────────┬────────┘
         │ P2P RX
         ▼
┌─────────────────┐
│  LoRa RX        │  빠르게 반환!
│  Callback       │
└────────┬────────┘
         │ lora_gps_bridge_send()
         │ (non-blocking)
         ▼
┌─────────────────┐
│   Packet Queue  │  깊이: 20
│   (FreeRTOS)    │  최대 ~20초 버퍼링
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Bridge Task    │  우선순위: 높음
│  (Forwarding)   │  별도 태스크
└────────┬────────┘
         │ gps_send_raw_data()
         │ (mutex-protected)
         ▼
┌─────────────────┐
│  GPS UART TX    │  뮤텍스로 보호
│  (gps_uart2_    │  순차 실행 보장
│   send)         │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   GPS Module    │
│   (UM982/F9P)   │
└─────────────────┘
```

## 사용 방법

### 1. 초기화

```c
#include "lora.h"
#include "lora_to_gps_bridge.h"
#include "gps_app.h"

// 1. GPS 초기화 (먼저!)
gps_init_all();

// 2. LoRa 초기화
lora_t g_lora;
lora_init(&g_lora, &huart3);

// 3. LoRa 수신 콜백 등록
lora_set_p2p_rx_callback(&g_lora, my_lora_rx_callback);

// 4. 브릿지 초기화
bool ok = lora_gps_bridge_init(GPS_ID_BASE);
if (!ok) {
  printf("Bridge init failed!\n");
}

// 5. LoRa P2P 모드 설정
lora_set_p2p_mode(&g_lora, true);

lora_p2p_config_t config = {
  .frequency = 922000000,
  .spreading_factor = 7,
  .bandwidth = 0,
  .coding_rate = 1,
  .preamble_length = 8,
  .tx_power = 14
};
lora_configure_p2p(&g_lora, &config);

// 6. 수신 시작
lora_p2p_start_rx(&g_lora, 0);  // 연속 수신
```

### 2. LoRa 수신 콜백 구현

```c
void my_lora_rx_callback(lora_t *lora, lora_p2p_rx_data_t *rx_data) {
  printf("LoRa RX: %u bytes, RSSI: %d dBm\n", rx_data->len, rx_data->rssi);

  // 브릿지로 전달 (논블로킹!)
  bool ok = lora_gps_bridge_send(rx_data->data, rx_data->len);

  if (!ok) {
    printf("Bridge queue full or no buffer!\n");
  }

  // 콜백은 즉시 반환됩니다
}
```

### 3. 통계 모니터링

```c
void monitor_task(void *param) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    lora_gps_stats_t stats;
    lora_gps_bridge_get_stats(&stats);

    printf("=== Bridge Stats ===\n");
    printf("RX: %lu packets, %lu bytes\n",
           stats.packets_received, stats.bytes_received);
    printf("TX: %lu packets, %lu bytes\n",
           stats.packets_sent, stats.bytes_sent);
    printf("Dropped: %lu packets\n", stats.packets_dropped);
    printf("Queue: %lu/%d (high: %lu)\n",
           lora_gps_bridge_get_queue_count(),
           LORA_GPS_QUEUE_SIZE,
           stats.queue_high_water);
  }
}
```

## API 레퍼런스

### 초기화 및 종료

#### `lora_gps_bridge_init()`
```c
bool lora_gps_bridge_init(gps_id_t gps_id);
```
- **설명**: 브릿지 초기화 (큐, 버퍼 풀, 전달 태스크 생성)
- **파라미터**: `gps_id` - 대상 GPS ID (GPS_ID_BASE 또는 GPS_ID_ROVER)
- **반환**: 성공 시 `true`, 실패 시 `false`
- **주의**: GPS 초기화 후에 호출해야 함

#### `lora_gps_bridge_deinit()`
```c
void lora_gps_bridge_deinit(void);
```
- **설명**: 브릿지 종료 및 리소스 해제

### 데이터 전송

#### `lora_gps_bridge_send()`
```c
bool lora_gps_bridge_send(const uint8_t *data, size_t len);
```
- **설명**: LoRa 수신 데이터를 GPS로 전달 (논블로킹)
- **파라미터**:
  - `data` - 전송할 데이터 포인터
  - `len` - 데이터 길이 (최대 `LORA_GPS_BUFFER_SIZE` = 1024 bytes)
- **반환**: 성공 시 `true`, 큐가 가득 차거나 버퍼 없으면 `false`
- **주의**: LoRa 수신 콜백에서 안전하게 호출 가능 (빠르게 반환)

### 통계 및 모니터링

#### `lora_gps_bridge_get_stats()`
```c
void lora_gps_bridge_get_stats(lora_gps_stats_t *stats);
```
- **설명**: 브릿지 통계 조회
- **통계 항목**:
  - `packets_received` - 수신한 총 패킷 수
  - `packets_sent` - GPS로 전송한 총 패킷 수
  - `packets_dropped` - 드롭된 패킷 수 (큐 풀 또는 버퍼 부족)
  - `bytes_received` - 수신한 총 바이트 수
  - `bytes_sent` - 전송한 총 바이트 수
  - `queue_high_water` - 큐 사용량 최대치
  - `alloc_failures` - 버퍼 할당 실패 횟수

#### `lora_gps_bridge_reset_stats()`
```c
void lora_gps_bridge_reset_stats(void);
```
- **설명**: 통계 카운터 리셋

#### `lora_gps_bridge_get_queue_count()`
```c
uint32_t lora_gps_bridge_get_queue_count(void);
```
- **설명**: 현재 큐에 대기 중인 패킷 수 조회
- **반환**: 대기 중인 패킷 수

## 설정 및 튜닝

### 기본 설정 (lora_to_gps_bridge.h)

```c
#define LORA_GPS_QUEUE_SIZE         20      // 큐 깊이
#define LORA_GPS_BUFFER_SIZE        1024    // 최대 패킷 크기
#define LORA_GPS_BUFFER_POOL_SIZE   10      // 버퍼 풀 크기
```

### 성능 튜닝 가이드

#### 매초 1KB 수신 시나리오 (기본 설정 충분)

- **큐 크기**: 20개 → 최대 ~20초 버퍼링 가능
- **버퍼 풀**: 10개 → 충분함
- **메모리 사용량**: ~10KB (버퍼 풀) + ~1KB (큐 오버헤드) = **약 11KB**

#### 더 높은 throughput이 필요한 경우

**시나리오: 매초 5KB 수신**

```c
// lora_to_gps_bridge.h 수정
#define LORA_GPS_QUEUE_SIZE         30      // 증가
#define LORA_GPS_BUFFER_SIZE        2048    // 패킷 크기 증가
#define LORA_GPS_BUFFER_POOL_SIZE   15      // 버퍼 수 증가
```

메모리 사용량: ~15 * 2KB = **약 30KB**

#### 큐 크기 결정 방법

공식: `큐 크기 = (최대 지연 시간(초) * 수신 속도(packets/sec)) * 안전 계수`

예:
- 최대 지연 허용: 10초
- 수신 속도: 1 packet/sec
- 안전 계수: 2x

→ 큐 크기 = 10 * 1 * 2 = **20개**

### 태스크 우선순위

브릿지 전달 태스크 우선순위: **`tskIDLE_PRIORITY + 3`** (높음)

이유:
- GPS로 빠르게 전송하여 큐가 차지 않도록 함
- LoRa 수신보다 높은 우선순위로 지연 최소화

필요 시 조정:
```c
// lora_to_gps_bridge.c의 lora_gps_bridge_init() 참조
xTaskCreate(
  lora_gps_forward_task,
  "lora_gps_fwd",
  512,
  NULL,
  tskIDLE_PRIORITY + 3,  // ← 여기 조정
  &g_bridge_ctx.forward_task
);
```

## 에러 처리

### 1. 패킷 드롭 (Queue Full)

**증상**: `packets_dropped` 증가

**원인**:
- GPS 전송 속도보다 LoRa 수신 속도가 빠름
- 큐가 가득 참

**해결**:
1. `LORA_GPS_QUEUE_SIZE` 증가
2. 브릿지 태스크 우선순위 상승
3. GPS UART 속도 확인 (현재 115200 bps)

### 2. 버퍼 할당 실패

**증상**: `alloc_failures` 증가

**원인**:
- 버퍼 풀이 부족함
- 버퍼가 회수되지 않음 (버그)

**해결**:
1. `LORA_GPS_BUFFER_POOL_SIZE` 증가
2. 통계로 실제 사용량 확인

### 3. 큐 사용량이 계속 높음

**증상**: `queue_high_water`가 `LORA_GPS_QUEUE_SIZE`에 근접

**원인**:
- GPS 전송 속도가 수신 속도를 따라가지 못함

**해결**:
1. GPS UART baud rate 증가 (115200 → 230400 등)
2. 브릿지 태스크 우선순위 상승
3. 큐 크기 증가

## 디버깅

### 로그 활성화

```c
// lora_to_gps_bridge.c에서 LOG_DEBUG 활성화
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
```

### 주요 로그 메시지

```
[Bridge] Data queued successfully (512 bytes)  // 정상
[Bridge] Failed to queue data (queue full)     // 큐 풀
[Bridge] No buffer available                   // 버퍼 풀
Forwarded 512 bytes to GPS[0]                  // 전송 성공
Failed to send to GPS[0]                       // 전송 실패
```

### 통계 출력 예제

```c
void print_stats(void) {
  lora_gps_stats_t stats;
  lora_gps_bridge_get_stats(&stats);

  printf("=== Bridge Statistics ===\n");
  printf("RX: %lu pkts, %lu bytes\n",
         stats.packets_received, stats.bytes_received);
  printf("TX: %lu pkts, %lu bytes\n",
         stats.packets_sent, stats.bytes_sent);
  printf("Dropped: %lu pkts (%.2f%%)\n",
         stats.packets_dropped,
         stats.packets_received > 0 ?
         (float)stats.packets_dropped / stats.packets_received * 100 : 0);
  printf("Queue: %lu (high: %lu/%d)\n",
         lora_gps_bridge_get_queue_count(),
         stats.queue_high_water,
         LORA_GPS_QUEUE_SIZE);
  printf("Buffer alloc fails: %lu\n", stats.alloc_failures);
  printf("========================\n");
}
```

## 예제 코드

완전한 예제는 `lora_gps_integration_example.c` 참조

### 기본 사용 예제

```c
#include "lora.h"
#include "lora_to_gps_bridge.h"
#include "gps_app.h"

static lora_t g_lora;

void lora_rx_callback(lora_t *lora, lora_p2p_rx_data_t *rx_data) {
  // GPS로 전달
  lora_gps_bridge_send(rx_data->data, rx_data->len);
}

void app_main(void) {
  // 초기화
  gps_init_all();
  lora_init(&g_lora, &huart3);
  lora_set_p2p_rx_callback(&g_lora, lora_rx_callback);
  lora_gps_bridge_init(GPS_ID_BASE);

  // P2P 설정
  lora_set_p2p_mode(&g_lora, true);
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
  lora_p2p_start_rx(&g_lora, 0);

  // 완료!
}
```

## 테스트

### 1. 단위 테스트

```c
// 브릿지 초기화 테스트
void test_bridge_init(void) {
  bool ok = lora_gps_bridge_init(GPS_ID_BASE);
  assert(ok == true);
  lora_gps_bridge_deinit();
}

// 데이터 전송 테스트
void test_data_send(void) {
  uint8_t test_data[100] = {1, 2, 3, 4, 5};
  bool ok = lora_gps_bridge_send(test_data, sizeof(test_data));
  assert(ok == true);
}
```

### 2. 통합 테스트

```c
// LoRa RX → GPS TX 전체 경로 테스트
void test_end_to_end(void) {
  // 초기화
  gps_init_all();
  lora_init(&g_lora, &huart3);
  lora_gps_bridge_init(GPS_ID_BASE);

  // 테스트 데이터 전송
  uint8_t test_data[1000];
  memset(test_data, 0xAA, sizeof(test_data));

  bool ok = lora_gps_bridge_send(test_data, sizeof(test_data));
  assert(ok == true);

  // 잠시 대기
  vTaskDelay(pdMS_TO_TICKS(100));

  // 통계 확인
  lora_gps_stats_t stats;
  lora_gps_bridge_get_stats(&stats);
  assert(stats.packets_sent == 1);
  assert(stats.bytes_sent == sizeof(test_data));
}
```

## FAQ

### Q1: 왜 별도 태스크를 사용하나요?

**A**: LoRa 수신 콜백에서 직접 GPS로 전송하면:
- 콜백이 오래 블로킹됨 (1KB 송신 시 ~100ms)
- 다음 LoRa 수신을 놓칠 수 있음
- 별도 태스크 + 큐 방식으로 콜백은 빠르게 반환

### Q2: 큐가 가득 차면 어떻게 되나요?

**A**: `lora_gps_bridge_send()`가 `false`를 반환하고 패킷이 드롭됩니다.
`packets_dropped` 통계가 증가하며, 큐 크기를 늘려야 합니다.

### Q3: 버퍼 풀은 왜 사용하나요?

**A**: 매 수신마다 동적 할당하면:
- 메모리 파편화 발생
- 할당 실패 위험
- 버퍼 풀로 미리 할당하여 재사용하면 안정적

### Q4: GPS UART 뮤텍스는 성능에 영향을 주나요?

**A**: 영향은 미미합니다:
- 뮤텍스 획득/해제는 매우 빠름 (~수 μs)
- UART 송신 시간이 지배적 (1KB @ 115200 bps = ~87ms)
- 안전성 대비 성능 영향은 무시할 수 있음

### Q5: 매초 1KB 이상 처리할 수 있나요?

**A**: 가능합니다:
- UART 속도: 115200 bps = ~11.5 KB/s 이론값
- 실제 ~10 KB/s 가능
- 더 필요하면 baud rate 증가 (230400, 460800 등)

## 참고 자료

- LoRa P2P 사용법: `lib/lora/RAK4270_USAGE.md`
- GPS 모듈 사용법: GPS 관련 문서
- FreeRTOS 큐: [FreeRTOS Queue Documentation](https://www.freertos.org/a00018.html)

## 라이센스

프로젝트 라이센스를 따릅니다.
