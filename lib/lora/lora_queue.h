#ifndef LORA_QUEUE_H
#define LORA_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// LoRa 전송 청크 설정
#define LORA_MAX_CHUNK_SIZE 128        // LoRa 패킷당 최대 크기 (조정 가능)
#define LORA_CHUNK_HEADER_SIZE 4       // 청크 헤더 크기
#define LORA_CHUNK_PAYLOAD_SIZE (LORA_MAX_CHUNK_SIZE - LORA_CHUNK_HEADER_SIZE)
#define LORA_QUEUE_SIZE 10             // 큐에 저장 가능한 최대 RTCM 패킷 수

// LoRa 청크 헤더 구조
typedef struct __attribute__((packed)) {
    uint8_t packet_id;      // 패킷 ID (0-255 순환)
    uint8_t chunk_index;    // 현재 청크 인덱스 (0부터 시작)
    uint8_t total_chunks;   // 총 청크 개수
    uint8_t reserved;       // 예약 (추후 확장용)
} lora_chunk_header_t;

// LoRa 청크 (헤더 + 페이로드)
typedef struct {
    lora_chunk_header_t header;
    uint8_t payload[LORA_CHUNK_PAYLOAD_SIZE];
    uint8_t payload_len;    // 실제 페이로드 길이
} lora_chunk_t;

// RTCM 패킷 큐 항목
typedef struct {
    uint8_t data[1029];     // RTCM3 최대 패킷 크기
    uint16_t length;        // 실제 패킷 길이
    uint8_t packet_id;      // 패킷 ID
    uint8_t current_chunk;  // 현재 전송 중인 청크 인덱스
    uint8_t total_chunks;   // 총 청크 개수
    bool completed;         // 전송 완료 여부
} lora_rtcm_packet_t;

// LoRa 큐 구조체
typedef struct {
    lora_rtcm_packet_t packets[LORA_QUEUE_SIZE];
    uint8_t head;           // 큐 헤드 (쓰기)
    uint8_t tail;           // 큐 테일 (읽기)
    uint8_t count;          // 현재 큐에 있는 패킷 수
    uint8_t next_packet_id; // 다음 패킷 ID (0-255 순환)
    uint32_t dropped_count; // 큐 풀로 드롭된 패킷 수
} lora_queue_t;

/**
 * @brief LoRa 큐 초기화
 * @param queue 큐 구조체 포인터
 */
void lora_queue_init(lora_queue_t *queue);

/**
 * @brief RTCM 패킷을 큐에 추가
 * @param queue 큐 구조체 포인터
 * @param rtcm_data RTCM 패킷 데이터
 * @param length 패킷 길이
 * @return true: 성공, false: 큐 풀
 */
bool lora_queue_enqueue(lora_queue_t *queue, const uint8_t *rtcm_data, uint16_t length);

/**
 * @brief 다음 전송할 청크 가져오기
 * @param queue 큐 구조체 포인터
 * @param chunk 출력 청크 포인터
 * @return true: 청크 있음, false: 전송할 청크 없음
 */
bool lora_queue_get_next_chunk(lora_queue_t *queue, lora_chunk_t *chunk);

/**
 * @brief 현재 패킷 전송 완료 처리 및 큐에서 제거
 * @param queue 큐 구조체 포인터
 */
void lora_queue_dequeue(lora_queue_t *queue);

/**
 * @brief 큐가 비어있는지 확인
 * @param queue 큐 구조체 포인터
 * @return true: 비어있음, false: 데이터 있음
 */
bool lora_queue_is_empty(lora_queue_t *queue);

/**
 * @brief 큐가 가득 찼는지 확인
 * @param queue 큐 구조체 포인터
 * @return true: 가득 참, false: 공간 있음
 */
bool lora_queue_is_full(lora_queue_t *queue);

/**
 * @brief 큐에 있는 패킷 수 반환
 * @param queue 큐 구조체 포인터
 * @return 큐에 있는 패킷 수
 */
uint8_t lora_queue_get_count(lora_queue_t *queue);

/**
 * @brief 청크를 바이트 배열로 직렬화
 * @param chunk 청크 구조체
 * @param out_buffer 출력 버퍼
 * @param out_len 출력 길이 포인터
 */
void lora_chunk_serialize(const lora_chunk_t *chunk, uint8_t *out_buffer, uint8_t *out_len);

#endif
