#include "lora_queue.h"
#include <string.h>

/**
 * @brief LoRa 큐 초기화
 */
void lora_queue_init(lora_queue_t *queue, QueueHandle_t notify_queue) {
    if (!queue) return;

    memset(queue, 0, sizeof(lora_queue_t));
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->next_packet_id = 0;
    queue->dropped_count = 0;
    queue->notify_queue = notify_queue;

    // 통계 초기화
    memset(&queue->stats, 0, sizeof(lora_queue_stats_t));
}

/**
 * @brief RTCM 패킷을 청크 개수로 계산
 */
static uint8_t calculate_chunks(uint16_t packet_length) {
    return (packet_length + LORA_CHUNK_PAYLOAD_SIZE - 1) / LORA_CHUNK_PAYLOAD_SIZE;
}

/**
 * @brief RTCM 패킷을 큐에 추가
 */
bool lora_queue_enqueue_rtcm(lora_queue_t *queue, const uint8_t *rtcm_data, uint16_t length) {
    if (!queue || !rtcm_data || length == 0 || length > 1029) {
        return false;
    }

    // 통계: 총 enqueue 시도 횟수
    queue->stats.total_enqueued++;

    // 큐가 가득 찬 경우
    if (lora_queue_is_full(queue)) {
        queue->dropped_count++;
        queue->stats.total_dropped++;
        return false;
    }

    // 새 패킷 추가
    lora_packet_t *pkt = &queue->packets[queue->head];
    pkt->type = LORA_PACKET_TYPE_RTCM;
    memcpy(pkt->data, rtcm_data, length);
    pkt->length = length;
    pkt->packet_id = queue->next_packet_id++;
    pkt->current_chunk = 0;
    pkt->total_chunks = calculate_chunks(length);
    pkt->completed = false;

    // 큐 헤드 이동
    queue->head = (queue->head + 1) % LORA_QUEUE_SIZE;
    queue->count++;

    // 통계: 현재 사용량 및 최대 사용량 업데이트
    queue->stats.current_usage = queue->count;
    if (queue->count > queue->stats.peak_usage) {
        queue->stats.peak_usage = queue->count;
    }

    // 전송 태스크에 신호 전송 (메시지큐 설정된 경우)
    if (queue->notify_queue) {
        uint8_t notify_msg = 1;
        xQueueSend(queue->notify_queue, &notify_msg, 0);  // 논블로킹
    }

    return true;
}

/**
 * @brief GPS 상태 패킷을 큐에 추가
 */
bool lora_queue_enqueue_status(lora_queue_t *queue, const lora_gps_status_t *status) {
    if (!queue || !status) {
        return false;
    }

    // 통계: 총 enqueue 시도 횟수
    queue->stats.total_enqueued++;

    // 큐가 가득 찬 경우
    if (lora_queue_is_full(queue)) {
        queue->dropped_count++;
        queue->stats.total_dropped++;
        return false;
    }

    // 새 패킷 추가
    lora_packet_t *pkt = &queue->packets[queue->head];
    pkt->type = LORA_PACKET_TYPE_STATUS;
    memcpy(pkt->data, status, sizeof(lora_gps_status_t));
    pkt->length = sizeof(lora_gps_status_t);
    pkt->packet_id = queue->next_packet_id++;
    pkt->current_chunk = 0;
    pkt->total_chunks = 1;  // STATUS 패킷은 항상 1개 청크 (작은 크기)
    pkt->completed = false;

    // 큐 헤드 이동
    queue->head = (queue->head + 1) % LORA_QUEUE_SIZE;
    queue->count++;

    // 통계: 현재 사용량 및 최대 사용량 업데이트
    queue->stats.current_usage = queue->count;
    if (queue->count > queue->stats.peak_usage) {
        queue->stats.peak_usage = queue->count;
    }

    // 전송 태스크에 신호 전송 (메시지큐 설정된 경우)
    if (queue->notify_queue) {
        uint8_t notify_msg = 1;
        xQueueSend(queue->notify_queue, &notify_msg, 0);  // 논블로킹
    }

    return true;
}

/**
 * @brief 다음 전송할 청크 가져오기
 */
bool lora_queue_get_next_chunk(lora_queue_t *queue, lora_chunk_t *chunk) {
    if (!queue || !chunk) return false;

    // 큐가 비어있으면 전송할 청크 없음
    if (lora_queue_is_empty(queue)) {
        return false;
    }

    lora_packet_t *pkt = &queue->packets[queue->tail];

    // 이미 전송 완료된 패킷
    if (pkt->completed) {
        lora_queue_dequeue(queue);
        return lora_queue_get_next_chunk(queue, chunk);
    }

    // 청크 헤더 설정
    chunk->header.packet_type = pkt->type;
    chunk->header.packet_id = pkt->packet_id;
    chunk->header.chunk_index = pkt->current_chunk;
    chunk->header.total_chunks = pkt->total_chunks;
    chunk->header.reserved = 0;

    // 청크 페이로드 계산
    uint16_t offset = pkt->current_chunk * LORA_CHUNK_PAYLOAD_SIZE;
    uint16_t remaining = pkt->length - offset;
    chunk->payload_len = (remaining > LORA_CHUNK_PAYLOAD_SIZE) ?
                         LORA_CHUNK_PAYLOAD_SIZE : remaining;

    memcpy(chunk->payload, &pkt->data[offset], chunk->payload_len);

    // 다음 청크로 이동
    pkt->current_chunk++;

    // 모든 청크 전송 완료
    if (pkt->current_chunk >= pkt->total_chunks) {
        pkt->completed = true;
    }

    return true;
}

/**
 * @brief 현재 패킷 전송 완료 처리 및 큐에서 제거
 */
void lora_queue_dequeue(lora_queue_t *queue) {
    if (!queue || lora_queue_is_empty(queue)) return;

    // 통계: 전송 완료 횟수
    queue->stats.total_transmitted++;

    // 큐 테일 이동
    queue->tail = (queue->tail + 1) % LORA_QUEUE_SIZE;
    queue->count--;

    // 통계: 현재 사용량 업데이트
    queue->stats.current_usage = queue->count;
}

/**
 * @brief 큐가 비어있는지 확인
 */
bool lora_queue_is_empty(lora_queue_t *queue) {
    if (!queue) return true;
    return (queue->count == 0);
}

/**
 * @brief 큐가 가득 찼는지 확인
 */
bool lora_queue_is_full(lora_queue_t *queue) {
    if (!queue) return true;
    return (queue->count >= LORA_QUEUE_SIZE);
}

/**
 * @brief 큐에 있는 패킷 수 반환
 */
uint8_t lora_queue_get_count(lora_queue_t *queue) {
    if (!queue) return 0;
    return queue->count;
}

/**
 * @brief 청크를 바이트 배열로 직렬화
 */
void lora_chunk_serialize(const lora_chunk_t *chunk, uint8_t *out_buffer, uint8_t *out_len) {
    if (!chunk || !out_buffer || !out_len) return;

    // 헤더 복사
    out_buffer[0] = chunk->header.packet_type;
    out_buffer[1] = chunk->header.packet_id;
    out_buffer[2] = chunk->header.chunk_index;
    out_buffer[3] = chunk->header.total_chunks;
    out_buffer[4] = chunk->header.reserved;

    // 페이로드 복사
    memcpy(&out_buffer[5], chunk->payload, chunk->payload_len);

    *out_len = LORA_CHUNK_HEADER_SIZE + chunk->payload_len;
}

/**
 * @brief 큐 사용률 반환 (0-100%)
 */
uint8_t lora_queue_get_usage_percent(lora_queue_t *queue) {
    if (!queue) return 0;
    return (queue->count * 100) / LORA_QUEUE_SIZE;
}

/**
 * @brief 큐가 경고 임계값을 초과했는지 확인
 */
bool lora_queue_is_warning(lora_queue_t *queue) {
    if (!queue) return false;
    return lora_queue_get_usage_percent(queue) >= LORA_QUEUE_WARNING_THRESHOLD;
}

/**
 * @brief 큐 통계 정보 반환
 */
const lora_queue_stats_t* lora_queue_get_stats(lora_queue_t *queue) {
    if (!queue) return NULL;
    return &queue->stats;
}

/**
 * @brief 큐 통계 초기화 (dropped count 등)
 */
void lora_queue_reset_stats(lora_queue_t *queue) {
    if (!queue) return;
    memset(&queue->stats, 0, sizeof(lora_queue_stats_t));
    queue->dropped_count = 0;
}
