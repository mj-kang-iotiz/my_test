#include "lora_queue.h"
#include <string.h>

/**
 * @brief LoRa 큐 초기화
 */
void lora_queue_init(lora_queue_t *queue) {
    if (!queue) return;

    memset(queue, 0, sizeof(lora_queue_t));
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->next_packet_id = 0;
    queue->dropped_count = 0;
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
bool lora_queue_enqueue(lora_queue_t *queue, const uint8_t *rtcm_data, uint16_t length) {
    if (!queue || !rtcm_data || length == 0 || length > 1029) {
        return false;
    }

    // 큐가 가득 찬 경우
    if (lora_queue_is_full(queue)) {
        queue->dropped_count++;
        return false;
    }

    // 새 패킷 추가
    lora_rtcm_packet_t *pkt = &queue->packets[queue->head];
    memcpy(pkt->data, rtcm_data, length);
    pkt->length = length;
    pkt->packet_id = queue->next_packet_id++;
    pkt->current_chunk = 0;
    pkt->total_chunks = calculate_chunks(length);
    pkt->completed = false;

    // 큐 헤드 이동
    queue->head = (queue->head + 1) % LORA_QUEUE_SIZE;
    queue->count++;

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

    lora_rtcm_packet_t *pkt = &queue->packets[queue->tail];

    // 이미 전송 완료된 패킷
    if (pkt->completed) {
        lora_queue_dequeue(queue);
        return lora_queue_get_next_chunk(queue, chunk);
    }

    // 청크 헤더 설정
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

    // 큐 테일 이동
    queue->tail = (queue->tail + 1) % LORA_QUEUE_SIZE;
    queue->count--;
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
    out_buffer[0] = chunk->header.packet_id;
    out_buffer[1] = chunk->header.chunk_index;
    out_buffer[2] = chunk->header.total_chunks;
    out_buffer[3] = chunk->header.reserved;

    // 페이로드 복사
    memcpy(&out_buffer[4], chunk->payload, chunk->payload_len);

    *out_len = LORA_CHUNK_HEADER_SIZE + chunk->payload_len;
}
