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

## LoRa 전송 시스템

### 1. 큐 초기화

```c
#include "lora_queue.h"

lora_queue_t lora_queue;
lora_queue_init(&lora_queue);
```

### 2. RTCM 패킷을 큐에 추가

```c
// RTCM 패킷을 큐에 추가 (자동으로 청킹됨)
if (!lora_queue_enqueue(&lora_queue, rtcm_data, rtcm_len)) {
    LOG_WARN("LoRa 큐가 가득 참! 패킷 드롭됨");
}
```

### 3. LoRa 전송 루프

```c
void lora_transmit_task(void) {
    lora_chunk_t chunk;
    uint8_t tx_buffer[LORA_MAX_CHUNK_SIZE];
    uint8_t tx_len;

    while (1) {
        // 큐에서 다음 청크 가져오기
        if (lora_queue_get_next_chunk(&lora_queue, &chunk)) {
            // 청크를 바이트 배열로 직렬화
            lora_chunk_serialize(&chunk, tx_buffer, &tx_len);

            // LoRa로 전송
            lora_send(tx_buffer, tx_len);

            LOG_INFO("LoRa TX: Packet %d, Chunk %d/%d (%d bytes)",
                     chunk.header.packet_id,
                     chunk.header.chunk_index + 1,
                     chunk.header.total_chunks,
                     tx_len);
        } else {
            // 큐가 비어있음, 대기
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
```

### 4. LoRa 수신 및 재조립 (Rover 측)

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
    // 헤더 파싱
    uint8_t packet_id = data[0];
    uint8_t chunk_index = data[1];
    uint8_t total_chunks = data[2];
    uint8_t payload_len = len - LORA_CHUNK_HEADER_SIZE;

    // 재조립 버퍼에 청크 저장
    rtcm_reassembly_t *reasm = &reassembly[packet_id];

    if (chunk_index == 0) {
        // 첫 청크: 초기화
        memset(reasm, 0, sizeof(rtcm_reassembly_t));
        reasm->total_chunks = total_chunks;
    }

    // 페이로드 복사
    uint16_t offset = chunk_index * LORA_CHUNK_PAYLOAD_SIZE;
    memcpy(&reasm->buffer[offset], &data[LORA_CHUNK_HEADER_SIZE], payload_len);
    reasm->length += payload_len;
    reasm->received_chunks++;

    // 모든 청크 수신 완료?
    if (reasm->received_chunks >= reasm->total_chunks) {
        // RTCM 패킷 재조립 완료!
        LOG_INFO("RTCM Packet %d 재조립 완료 (%d bytes)", packet_id, reasm->length);

        // GPS로 전송
        gps_send_rtcm(reasm->buffer, reasm->length);

        reasm->complete = true;
    }
}
```

## 청킹 프로토콜 상세

### LoRa 청크 구조

```
┌──────────────┬──────────────┬──────────────┬──────────────┬─────────────────┐
│ Packet ID    │ Chunk Index  │ Total Chunks │ Reserved     │ Payload         │
│ (1 byte)     │ (1 byte)     │ (1 byte)     │ (1 byte)     │ (0-124 bytes)   │
└──────────────┴──────────────┴──────────────┴──────────────┴─────────────────┘
```

- **Packet ID**: 0-255 순환, 패킷 구분용
- **Chunk Index**: 현재 청크 인덱스 (0부터 시작)
- **Total Chunks**: 전체 청크 개수
- **Reserved**: 예약됨 (추후 확장용)
- **Payload**: 실제 RTCM 데이터 (최대 124바이트)

### 청킹 예시

1029바이트 RTCM 패킷을 전송하는 경우:
- Chunk 0: 124 bytes
- Chunk 1: 124 bytes
- ...
- Chunk 7: 124 bytes
- Chunk 8: 37 bytes (나머지)

**총 9개 청크** (1029 / 124 = 8.3 → 9 청크)

## 성능 최적화

### 1. 큐 크기 조정

```c
// lora_queue.h에서 조정
#define LORA_QUEUE_SIZE 10  // 동시에 버퍼링할 RTCM 패킷 수
```

### 2. 청크 크기 조정

```c
// lora_queue.h에서 조정
#define LORA_MAX_CHUNK_SIZE 128  // LoRa 모듈의 최대 패킷 크기에 맞춤
```

### 3. 전송 속도 제어

LoRa 속도가 느린 경우:
- 큐 크기를 늘려 버퍼링
- 중요하지 않은 RTCM 메시지 타입 필터링 (GNSS 모듈에서 처리)
- 전송 간격 조정

## 디버깅

### RTCM 파싱 통계

```c
rtcm_parser_t *parser = &gps->rtcm;
LOG_INFO("RTCM 통계:");
LOG_INFO("  파싱된 패킷: %lu", parser->packet_count);
LOG_INFO("  에러 카운트: %lu", parser->error_count);
```

### LoRa 큐 상태

```c
LOG_INFO("LoRa 큐 상태:");
LOG_INFO("  현재 패킷 수: %d", lora_queue_get_count(&lora_queue));
LOG_INFO("  드롭된 패킷: %lu", lora_queue.dropped_count);
```

## 주의사항

1. **CRC 검증**: RTCM 패킷은 CRC24로 검증되므로 손상된 패킷은 자동으로 폐기됩니다
2. **큐 오버플로**: LoRa 전송 속도가 RTCM 수신 속도보다 느리면 큐가 가득 찰 수 있습니다
3. **순서 보장**: Packet ID는 256에서 순환하므로, 장시간 운영 시 중복 가능성을 고려해야 합니다
4. **청크 손실**: LoRa 전송 중 청크가 손실되면 전체 RTCM 패킷을 재조립할 수 없습니다 (재전송 로직 필요)
