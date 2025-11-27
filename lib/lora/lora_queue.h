#ifndef LORA_QUEUE_H
#define LORA_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "queue.h"

// LoRa 전송 청크 설정
// 240바이트까지 전송 가능한 경우 아래 값을 240으로 변경
#define LORA_MAX_CHUNK_SIZE 240        // LoRa 패킷당 최대 크기 (128, 240 등)
#define LORA_CHUNK_HEADER_SIZE 5       // 청크 헤더 크기 (패킷 타입 추가)
#define LORA_CHUNK_PAYLOAD_SIZE (LORA_MAX_CHUNK_SIZE - LORA_CHUNK_HEADER_SIZE)

// LoRa 큐 크기 설정
// 메모리 제약: 각 패킷 최대 1029 bytes
// - 10개: 약 10KB RAM
// - 20개: 약 20KB RAM (권장: 일반적인 경우)
// - 30개: 약 30KB RAM (권장: LoRa 속도가 매우 느린 경우, SF11-12)
#define LORA_QUEUE_SIZE 20             // 큐에 저장 가능한 최대 RTCM 패킷 수

// Overflow 경고 임계값 (큐 사용률 %)
#define LORA_QUEUE_WARNING_THRESHOLD 80  // 80% 이상 사용 시 경고

// LoRa 패킷 타입
typedef enum {
    LORA_PACKET_TYPE_RTCM = 0x01,      // RTCM 패킷 (청킹 필요)
    LORA_PACKET_TYPE_STATUS = 0x02,    // GPS 상태 패킷 (작은 크기, 청킹 불필요)
} lora_packet_type_t;

// LoRa 청크 헤더 구조
typedef struct __attribute__((packed)) {
    uint8_t packet_type;    // 패킷 타입 (RTCM, STATUS 등)
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

// GPS fix 상태 패킷 구조 (10초마다 전송)
typedef struct __attribute__((packed)) {
    uint8_t fix_type;       // GPS fix 타입 (0=NO_FIX, 1=GPS, 4=RTK_FIX, 5=RTK_FLOAT)
    uint8_t num_satellites; // 위성 개수
    uint16_t hdop;          // HDOP * 100 (예: 1.23 → 123)
    int32_t latitude;       // 위도 * 1e7 (도 단위)
    int32_t longitude;      // 경도 * 1e7 (도 단위)
    int32_t altitude;       // 고도 * 1000 (mm 단위)
} lora_gps_status_t;

// LoRa 패킷 큐 항목
typedef struct {
    lora_packet_type_t type;  // 패킷 타입
    uint8_t data[1029];       // 패킷 데이터 (RTCM 또는 STATUS)
    uint16_t length;          // 실제 패킷 길이
    uint8_t packet_id;        // 패킷 ID
    uint8_t current_chunk;    // 현재 전송 중인 청크 인덱스
    uint8_t total_chunks;     // 총 청크 개수
    bool completed;           // 전송 완료 여부
} lora_packet_t;

// LoRa 큐 통계
typedef struct {
    uint32_t total_enqueued;    // 총 큐 추가 시도 횟수
    uint32_t total_dropped;     // 큐 풀로 드롭된 패킷 수
    uint32_t total_transmitted; // 전송 완료된 패킷 수
    uint8_t peak_usage;         // 최대 큐 사용량 (패킷 수)
    uint8_t current_usage;      // 현재 큐 사용량 (패킷 수)
} lora_queue_stats_t;

// LoRa 큐 구조체
typedef struct {
    lora_packet_t packets[LORA_QUEUE_SIZE];
    uint8_t head;           // 큐 헤드 (쓰기)
    uint8_t tail;           // 큐 테일 (읽기)
    uint8_t count;          // 현재 큐에 있는 패킷 수
    uint8_t next_packet_id; // 다음 패킷 ID (0-255 순환)
    uint32_t dropped_count; // 큐 풀로 드롭된 패킷 수 (deprecated, use stats.total_dropped)
    QueueHandle_t notify_queue;  // 전송 태스크 알림용 메시지큐
    lora_queue_stats_t stats;    // 큐 통계 정보
} lora_queue_t;

/**
 * @brief LoRa 큐 초기화
 * @param queue 큐 구조체 포인터
 * @param notify_queue 전송 태스크 알림용 FreeRTOS 메시지큐 (NULL이면 폴링 모드)
 */
void lora_queue_init(lora_queue_t *queue, QueueHandle_t notify_queue);

/**
 * @brief RTCM 패킷을 큐에 추가
 * @param queue 큐 구조체 포인터
 * @param rtcm_data RTCM 패킷 데이터
 * @param length 패킷 길이
 * @return true: 성공, false: 큐 풀
 */
bool lora_queue_enqueue_rtcm(lora_queue_t *queue, const uint8_t *rtcm_data, uint16_t length);

/**
 * @brief GPS 상태 패킷을 큐에 추가
 * @param queue 큐 구조체 포인터
 * @param status GPS 상태 구조체
 * @return true: 성공, false: 큐 풀
 */
bool lora_queue_enqueue_status(lora_queue_t *queue, const lora_gps_status_t *status);

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

/**
 * @brief 큐 사용률 반환 (0-100%)
 * @param queue 큐 구조체 포인터
 * @return 큐 사용률 (퍼센트)
 */
uint8_t lora_queue_get_usage_percent(lora_queue_t *queue);

/**
 * @brief 큐가 경고 임계값을 초과했는지 확인
 * @param queue 큐 구조체 포인터
 * @return true: 경고 상태, false: 정상
 */
bool lora_queue_is_warning(lora_queue_t *queue);

/**
 * @brief 큐 통계 정보 반환
 * @param queue 큐 구조체 포인터
 * @return 통계 구조체 포인터
 */
const lora_queue_stats_t* lora_queue_get_stats(lora_queue_t *queue);

/**
 * @brief 큐 통계 초기화 (dropped count 등)
 * @param queue 큐 구조체 포인터
 */
void lora_queue_reset_stats(lora_queue_t *queue);

#endif
