# RTCM 파싱 및 LoRa 전송 가이드

## 개요

이 문서는 RTCM3 패킷 파싱과 LoRa 전송을 위한 청킹/큐잉 시스템 사용법을 설명합니다.

## RTCM 패킷 파싱

### 1. RTCM 패킷 식별

RTCM3 프로토콜은 **0xD3**로 시작합니다.

```c
#include "rtcm.h"

// 간단한 RTCM 패킷 확인
uint8_t data[] = {0xD3, 0x00, 0x13, ...};
if (rtcm_is_packet(data, sizeof(data))) {
    // RTCM 패킷입니다
}
```

### 2. 스트림 데이터 파싱 (권장)

UART나 TCP/IP로 받은 스트림 데이터를 바이트 단위로 파싱:

```c
#include "rtcm.h"

rtcm_parser_t parser;
rtcm_parser_init(&parser);

// 바이트 단위로 데이터 수신
for (size_t i = 0; i < data_len; i++) {
    if (rtcm_parse_byte(&parser, data[i])) {
        // RTCM 패킷 파싱 완료!
        const uint8_t *packet = NULL;
        uint16_t packet_len = 0;

        rtcm_get_packet(&parser, &packet, &packet_len);

        // 패킷 처리 (예: LoRa 전송)
        process_rtcm_packet(packet, packet_len);

        // 다음 패킷을 위해 리셋
        rtcm_parser_reset(&parser);
    }
}
```

### 3. GPS 통합 파싱 (GGA + RTCM 자동 분리)

GPS 파서는 자동으로 NMEA, UBX, RTCM을 분리합니다:

```c
#include "gps.h"

void gps_evt_handler(gps_t* gps, gps_event_t event, gps_procotol_t protocol, gps_msg_t msg) {
    switch (protocol) {
        case GPS_PROTOCOL_NMEA:
            if (msg.nmea == GPS_NMEA_MSG_GGA) {
                // GGA 패킷 처리
                LOG_INFO("GGA: lat=%f, lon=%f", gps->nmea_data.gga.lat, gps->nmea_data.gga.lon);
            }
            break;

        case GPS_PROTOCOL_RTCM:
            if (event == GPS_EVENT_RTCM_PACKET) {
                // RTCM 패킷 파싱 완료
                const uint8_t *rtcm_packet = NULL;
                uint16_t rtcm_len = 0;
                rtcm_get_packet(&gps->rtcm, &rtcm_packet, &rtcm_len);

                // LoRa 큐에 추가
                lora_queue_enqueue(&lora_queue, rtcm_packet, rtcm_len);
            }
            break;
    }
}
```

## LoRa 패킷 타입

LoRa 큐는 두 가지 패킷 타입을 지원합니다:

1. **RTCM 패킷** (`LORA_PACKET_TYPE_RTCM = 0x01`)
   - NTRIP 서버에서 받은 RTCM3 보정 데이터
   - 크기: 최대 1029바이트
   - 청킹 필요

2. **GPS 상태 패킷** (`LORA_PACKET_TYPE_STATUS = 0x02`)
   - GPS fix 상태 (10초마다)
   - 크기: 18바이트 (고정)
   - 청킹 불필요 (1개 청크)

## LoRa 전송 시스템

### 1. 초기화 (메시지큐 방식 - 권장)

```c
#include "lora_queue.h"
#include "FreeRTOS.h"
#include "queue.h"

lora_queue_t lora_queue;
QueueHandle_t lora_notify_queue;

// 메시지큐 생성
lora_notify_queue = xQueueCreate(20, sizeof(uint8_t));

// LoRa 큐 초기화 (메시지큐 연결)
lora_queue_init(&lora_queue, lora_notify_queue);
```

### 2. 패킷을 큐에 추가

#### RTCM 패킷 추가
```c
// RTCM 패킷을 큐에 추가 (자동으로 청킹 + 메시지큐 신호 전송)
if (!lora_queue_enqueue_rtcm(&lora_queue, rtcm_data, rtcm_len)) {
    LOG_WARN("LoRa 큐가 가득 참! 패킷 드롭됨");
}
```

#### GPS 상태 패킷 추가 (10초마다)
```c
// GPS 상태 구조체 채우기
lora_gps_status_t status;
status.fix_type = gps->nmea_data.gga.fix;
status.num_satellites = gps->nmea_data.gga.sats;
status.hdop = (uint16_t)(gps->nmea_data.gga.hdop * 100);
status.latitude = (int32_t)(gps->nmea_data.gga.lat * 1e7);
status.longitude = (int32_t)(gps->nmea_data.gga.lon * 1e7);
status.altitude = (int32_t)(gps->nmea_data.gga.alt * 1000);

// LoRa 큐에 추가
if (!lora_queue_enqueue_status(&lora_queue, &status)) {
    LOG_WARN("GPS 상태 패킷 드롭");
}
```

#### 10초 타이머 설정
```c
// FreeRTOS 타이머로 10초마다 GPS 상태 전송
#include "timers.h"

void gps_status_timer_callback(TimerHandle_t xTimer) {
    gps_t *gps = (gps_t *)pvTimerGetTimerID(xTimer);
    // GPS 상태를 LoRa 큐에 추가
    send_gps_status_to_lora(gps);
}

// 타이머 생성 및 시작
TimerHandle_t timer = xTimerCreate("gps_status", pdMS_TO_TICKS(10000), pdTRUE, gps, gps_status_timer_callback);
xTimerStart(timer, 0);
```

### 3. LoRa 전송 태스크 (메시지큐 방식)

```c
void lora_transmit_task(void *pvParameter) {
    lora_chunk_t chunk;
    uint8_t tx_buffer[LORA_MAX_CHUNK_SIZE];
    uint8_t tx_len;
    uint8_t notify_msg;

    while (1) {
        // ✅ 메시지큐 대기 (블로킹 방식, 효율적!)
        if (xQueueReceive(lora_notify_queue, &notify_msg, portMAX_DELAY)) {
            // RTCM 패킷이 큐에 추가됨! 모든 청크 전송
            while (lora_queue_get_next_chunk(&lora_queue, &chunk)) {
                // 청크를 바이트 배열로 직렬화
                lora_chunk_serialize(&chunk, tx_buffer, &tx_len);

                // LoRa로 전송
                lora_hal_send(tx_buffer, tx_len);

                LOG_INFO("LoRa TX: Packet %d, Chunk %d/%d (%d bytes)",
                         chunk.header.packet_id,
                         chunk.header.chunk_index + 1,
                         chunk.header.total_chunks,
                         tx_len);

                // 청크 간 간격 (LoRa 속도에 맞춰 조정)
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
    }
}
```

**장점:**
- 불필요한 폴링 제거 (CPU 절약)
- RTCM 패킷 수신 즉시 전송 시작
- 블로킹 방식으로 전력 효율적

### 4. LoRa 수신 및 패킷 처리 (Rover 측)

```c
typedef struct {
    uint8_t buffer[1029];
    uint16_t length;
    uint8_t received_chunks;
    uint8_t total_chunks;
    bool complete;
} rtcm_reassembly_t;

rtcm_reassembly_t reassembly[256];  // Packet ID별 버퍼

void lora_receive_handler(uint8_t *data, uint8_t len) {
    if (len < LORA_CHUNK_HEADER_SIZE) return;

    // 헤더 파싱
    uint8_t packet_type = data[0];
    uint8_t packet_id = data[1];
    uint8_t chunk_index = data[2];
    uint8_t total_chunks = data[3];
    uint8_t *payload = &data[LORA_CHUNK_HEADER_SIZE];
    uint8_t payload_len = len - LORA_CHUNK_HEADER_SIZE;

    if (packet_type == LORA_PACKET_TYPE_RTCM) {
        // RTCM 패킷 재조립
        rtcm_reassembly_t *reasm = &reassembly[packet_id];

        if (chunk_index == 0) {
            // 첫 청크: 초기화
            memset(reasm, 0, sizeof(rtcm_reassembly_t));
            reasm->total_chunks = total_chunks;
        }

        // 페이로드 복사
        uint16_t offset = chunk_index * LORA_CHUNK_PAYLOAD_SIZE;
        memcpy(&reasm->buffer[offset], payload, payload_len);
        reasm->length += payload_len;
        reasm->received_chunks++;

        // 모든 청크 수신 완료?
        if (reasm->received_chunks >= reasm->total_chunks) {
            LOG_INFO("RTCM Packet %d 재조립 완료 (%d bytes)", packet_id, reasm->length);

            // GPS로 전송
            gps_send_rtcm(reasm->buffer, reasm->length);
            reasm->complete = true;
        }

    } else if (packet_type == LORA_PACKET_TYPE_STATUS) {
        // GPS 상태 패킷 (청킹 없음)
        lora_gps_status_t status;
        memcpy(&status, payload, sizeof(lora_gps_status_t));

        // 역변환
        double lat = status.latitude / 1e7;
        double lon = status.longitude / 1e7;
        double alt = status.altitude / 1000.0;
        double hdop = status.hdop / 100.0;

        LOG_INFO("GPS 상태 수신: fix=%d, sats=%d, HDOP=%.2f",
                 status.fix_type, status.num_satellites, hdop);
        LOG_INFO("  위치: %.7f, %.7f, %.3fm", lat, lon, alt);

        // UI 업데이트 등...
    }
}
```

## 청킹 프로토콜 상세

### LoRa 청크 구조

```
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬─────────────────┐
│ Packet Type  │ Packet ID    │ Chunk Index  │ Total Chunks │ Reserved     │ Payload         │
│ (1 byte)     │ (1 byte)     │ (1 byte)     │ (1 byte)     │ (1 byte)     │ (0-235 bytes)   │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴─────────────────┘
```

- **Packet Type**: 패킷 타입 (0x01=RTCM, 0x02=STATUS)
- **Packet ID**: 0-255 순환, 패킷 구분용
- **Chunk Index**: 현재 청크 인덱스 (0부터 시작)
- **Total Chunks**: 전체 청크 개수
- **Reserved**: 예약됨 (추후 확장용)
- **Payload**: 실제 데이터 (최대 235바이트)

### GPS 상태 패킷 구조 (18바이트)

```
┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ Fix Type     │ Satellites   │ HDOP         │ Latitude     │ Longitude    │ Altitude     │
│ (1 byte)     │ (1 byte)     │ (2 bytes)    │ (4 bytes)    │ (4 bytes)    │ (4 bytes)    │
└──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

- **Fix Type**: 0=NO_FIX, 1=GPS, 4=RTK_FIX, 5=RTK_FLOAT
- **Satellites**: 위성 개수
- **HDOP**: HDOP * 100 (예: 1.23 → 123)
- **Latitude**: 위도 * 1e7 (도 단위, int32)
- **Longitude**: 경도 * 1e7 (도 단위, int32)
- **Altitude**: 고도 * 1000 (mm 단위, int32)

### 청크 크기 설정

`lora_queue.h`에서 LoRa 모듈의 최대 전송 크기에 맞춰 조정:

```c
// 240바이트 전송 가능한 경우
#define LORA_MAX_CHUNK_SIZE 240

// 128바이트만 가능한 경우
#define LORA_MAX_CHUNK_SIZE 128
```

### 청킹 예시 (240바이트 청크)

1029바이트 RTCM 패킷을 전송하는 경우:
- Chunk 0: 236 bytes
- Chunk 1: 236 bytes
- Chunk 2: 236 bytes
- Chunk 3: 236 bytes
- Chunk 4: 85 bytes (나머지)

**총 5개 청크** (1029 / 236 = 4.4 → 5 청크)

### 청킹 예시 (128바이트 청크)

1029바이트 RTCM 패킷을 전송하는 경우:
- Chunk 0: 124 bytes
- Chunk 1: 124 bytes
- ...
- Chunk 7: 124 bytes
- Chunk 8: 37 bytes (나머지)

**총 9개 청크** (1029 / 124 = 8.3 → 9 청크)

**→ 240바이트 청크 사용 시 전송 횟수가 거의 절반!**

## 성능 최적화

### 1. 청크 크기 최대화 ⭐ (가장 중요!)

```c
// lora_queue.h에서 조정
#define LORA_MAX_CHUNK_SIZE 240  // LoRa 모듈의 최대 패킷 크기

// 240바이트 사용 시:
// - 1029바이트 RTCM → 5개 청크 (128바이트 대비 거의 절반)
// - 전송 시간 대폭 감소
```

### 2. 메시지큐 방식 사용 ⭐ (권장)

```c
// ❌ 폴링 방식 (비효율적)
while (1) {
    if (lora_queue_get_next_chunk(...)) {
        // 전송
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // CPU 낭비
}

// ✅ 메시지큐 방식 (효율적)
while (1) {
    xQueueReceive(notify_queue, &msg, portMAX_DELAY);  // 블로킹
    while (lora_queue_get_next_chunk(...)) {
        // 전송
    }
}
```

### 3. 큐 크기 조정 ⭐ (중요!)

```c
// lora_queue.h에서 조정
#define LORA_QUEUE_SIZE 20  // 동시에 버퍼링할 RTCM 패킷 수 (기본값: 20)
```

**메모리 영향**:
- 10개: 약 10KB RAM
- 20개: 약 20KB RAM (권장: 일반적인 경우)
- 30개: 약 30KB RAM (권장: LoRa 속도가 매우 느린 경우, SF11-12)

**큐 크기 결정 방법**:
```
필요한 큐 크기 = (RTCM 수신 속도 - LoRa 전송 속도) × 버퍼링 시간

예시) SF9, BW 125kHz 기준:
  - RTCM 수신: 5 packets/sec
  - LoRa 전송: 0.33 packets/sec (1 packet ≈ 3초)
  - 큐 부족: 4.67 packets/sec
  - 10초 버퍼: 4.67 × 10 = 47개 필요 → 현실적으로 불가능!

해결책:
  1. RTCM 메시지 타입 필터링 (GNSS 모듈 설정)
  2. LoRa SF 낮추기 (SF7-8 사용)
  3. 업데이트 주기 증가 (1초 → 5초)
```

### 4. 전송 간격 조정

```c
// 청크 간 간격 (LoRa 모듈 처리 속도에 맞춤)
vTaskDelay(pdMS_TO_TICKS(50));  // 50ms
```

**LoRa 속도가 느린 경우**:
- 큐 크기를 늘려 버퍼링 (20 → 30개)
- 청크 간 간격 감소 (50ms → 10ms) - LoRa 모듈이 허용하는 한
- **중요하지 않은 RTCM 메시지 타입 필터링** (GNSS 모듈에서 처리)
  - 예: MSM7 → MSM4로 변경 (데이터 크기 약 50% 감소)
  - GPS만 사용 (GLONASS, BDS 비활성화)

## 디버깅 및 모니터링

### 1. RTCM 파싱 통계

```c
rtcm_parser_t *parser = &gps->rtcm;
LOG_INFO("RTCM 통계:");
LOG_INFO("  파싱된 패킷: %lu", parser->packet_count);
LOG_INFO("  에러 카운트: %lu", parser->error_count);
```

### 2. LoRa 큐 상태 조회 ⭐ (NEW!)

```c
// 기본 정보
LOG_INFO("큐 사용: %d/%d (사용률: %d%%)",
         lora_queue_get_count(&lora_queue),
         LORA_QUEUE_SIZE,
         lora_queue_get_usage_percent(&lora_queue));

// 상세 통계
const lora_queue_stats_t *stats = lora_queue_get_stats(&lora_queue);
LOG_INFO("총 enqueue: %lu", stats->total_enqueued);
LOG_INFO("드롭 패킷: %lu (%.1f%%)",
         stats->total_dropped,
         (stats->total_dropped * 100.0f) / stats->total_enqueued);
LOG_INFO("전송 완료: %lu", stats->total_transmitted);
LOG_INFO("최대 사용량: %d/%d", stats->peak_usage, LORA_QUEUE_SIZE);

// 경고 상태 확인
if (lora_queue_is_warning(&lora_queue)) {
    LOG_WARN("⚠️ 큐 사용률 %d%% 초과!", LORA_QUEUE_WARNING_THRESHOLD);
}
```

### 3. 큐 오버플로우 자동 모니터링 (권장)

**방법 1: 전용 모니터링 태스크** (lib/lora/lora_example.c 참고)

```c
static void lora_monitor_task(void *pvParameter) {
    const lora_queue_stats_t *stats;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5초마다

        stats = lora_queue_get_stats(&g_lora_queue);
        uint8_t usage = lora_queue_get_usage_percent(&g_lora_queue);

        // 경고 임계값 초과 시 (기본 80%)
        if (lora_queue_is_warning(&g_lora_queue)) {
            LOG_WARN("⚠️ LoRa 큐 경고! 사용률 %d%%", usage);
        }

        // 패킷 드롭 발생 시
        if (stats->total_dropped > 0) {
            float drop_rate = (stats->total_dropped * 100.0f) / stats->total_enqueued;
            LOG_ERR("❌ RTCM 패킷 손실: %d / %d (%.1f%%)",
                    stats->total_dropped, stats->total_enqueued, drop_rate);
        }
    }
}
```

**방법 2: enqueue 실패 시 즉시 경고**

```c
if (!lora_queue_enqueue_rtcm(&g_lora_queue, rtcm_data, rtcm_len)) {
    LOG_WARN("❌ LoRa 큐 풀! RTCM 패킷 드롭 (사용률: %d%%)",
             lora_queue_get_usage_percent(&g_lora_queue));
}
```

### 4. 통계 초기화

```c
// 통계 리셋 (예: 테스트 후)
lora_queue_reset_stats(&lora_queue);
```

## 주의사항

### 1. CRC 검증
RTCM 패킷은 **CRC24**로 검증되므로 손상된 패킷은 자동으로 폐기됩니다.

### 2. 큐 오버플로우 및 데이터 손실 ⚠️

**문제**: LoRa 전송 속도가 RTCM 수신 속도보다 느리면 큐가 가득 차서 **데이터 손실** 발생

**시나리오 예시** (SF9, BW 125kHz):
```
RTCM 수신 속도: 5 packets/sec (NTRIP 서버)
LoRa 전송 속도: 0.33 packets/sec (1 packet ≈ 3초)
큐 크기: 20개

결과:
  - 약 4초 후 큐 가득 참
  - 이후 초당 4.67개 패킷 드롭
  - 1분에 약 280개 패킷 손실!
```

**해결 방법**:

1. **RTCM 메시지 타입 필터링** (가장 효과적!)
   - GNSS 모듈 설정에서 필요한 메시지만 활성화
   - MSM7 → MSM4로 변경 (크기 약 50% 감소)
   - GPS만 사용 (GLONASS, BDS 비활성화)

2. **LoRa SF 최적화**
   - SF9-12 → SF7-8로 낮추기 (전송 속도 5-10배 증가)
   - 단, 통신 거리는 감소

3. **업데이트 주기 증가**
   - NTRIP 업데이트: 1초 → 2-5초

4. **큐 크기 증가**
   - 20개 → 30개 (일시적 버스트 흡수용)
   - 메모리: 약 30KB RAM 필요

5. **모니터링 필수**
   - `lora_monitor_task()` 사용
   - 드롭률 5% 이상 시 즉시 조치

### 3. 순서 보장
Packet ID는 **0-255에서 순환**하므로, 장시간 운영 시 중복 가능성을 고려해야 합니다.

### 4. 동시성 안전
현재 구현은 **단일 Producer, 단일 Consumer** 구조입니다. 여러 태스크에서 동시에 enqueue/dequeue 시 **Mutex 보호 필요**.

### 5. LoRa 모듈 처리 속도
청크 간 간격(`vTaskDelay(50)`)은 LoRa 모듈의 **TX FIFO 크기**에 따라 조정 필요. 너무 빠르면 모듈이 처리 못함.

### 6. 청크 손실
LoRa 전송 중 청크가 손실되면 전체 RTCM 패킷을 재조립할 수 없습니다. 필요 시 재전송 로직 구현 필요.
