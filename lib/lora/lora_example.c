// LoRa 전송 예제 코드
#include "lora_queue.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// LoRa 큐 및 메시지큐
static lora_queue_t g_lora_queue;
static QueueHandle_t g_lora_notify_queue;

/**
 * @brief LoRa 초기화 및 태스크 생성
 */
void lora_init(void) {
    // 메시지큐 생성 (큐 크기: 20)
    g_lora_notify_queue = xQueueCreate(20, sizeof(uint8_t));
    if (g_lora_notify_queue == NULL) {
        LOG_ERR("LoRa 메시지큐 생성 실패");
        return;
    }

    // LoRa 큐 초기화 (메시지큐 연결)
    lora_queue_init(&g_lora_queue, g_lora_notify_queue);

    // LoRa 전송 태스크 생성
    xTaskCreate(lora_transmit_task, "lora_tx", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);

    LOG_INFO("LoRa 초기화 완료 (청크 크기: %d bytes)", LORA_MAX_CHUNK_SIZE);
}

/**
 * @brief LoRa 전송 태스크 (메시지큐 방식)
 */
static void lora_transmit_task(void *pvParameter) {
    lora_chunk_t chunk;
    uint8_t tx_buffer[LORA_MAX_CHUNK_SIZE];
    uint8_t tx_len;
    uint8_t notify_msg;

    LOG_INFO("LoRa 전송 태스크 시작");

    while (1) {
        // ✅ 메시지큐 대기 (블로킹 방식, 폴링 제거!)
        if (xQueueReceive(g_lora_notify_queue, &notify_msg, portMAX_DELAY)) {
            // RTCM 패킷이 큐에 추가됨! 모든 청크 전송
            while (lora_queue_get_next_chunk(&g_lora_queue, &chunk)) {
                // 청크를 바이트 배열로 직렬화
                lora_chunk_serialize(&chunk, tx_buffer, &tx_len);

                // LoRa 모듈로 전송
                int ret = lora_hal_send(tx_buffer, tx_len);
                if (ret < 0) {
                    LOG_ERR("LoRa 전송 실패: %d", ret);
                    break;
                }

                LOG_DEBUG("LoRa TX: Pkt=%d, Chunk=%d/%d, Size=%d",
                         chunk.header.packet_id,
                         chunk.header.chunk_index + 1,
                         chunk.header.total_chunks,
                         tx_len);

                // ⚠️ 청크 간 간격 (LoRa 모듈 속도에 맞춰 조정)
                // 너무 빠르게 전송하면 LoRa 모듈이 처리 못함
                vTaskDelay(pdMS_TO_TICKS(50));  // 50ms 간격
            }

            LOG_INFO("LoRa 패킷 전송 완료");
        }
    }
}

/**
 * @brief GPS 이벤트 핸들러 (RTCM 패킷을 LoRa 큐에 추가)
 */
void gps_evt_handler(gps_t* gps, gps_event_t event, gps_procotol_t protocol, gps_msg_t msg) {
    // ... (기존 GGA 처리 등)

    // RTCM 패킷 처리
    if (protocol == GPS_PROTOCOL_RTCM && event == GPS_EVENT_RTCM_PACKET) {
        const uint8_t *rtcm_data = NULL;
        uint16_t rtcm_len = 0;

        // RTCM 파서에서 패킷 가져오기
        rtcm_get_packet(&gps->rtcm, &rtcm_data, &rtcm_len);

        if (rtcm_data) {
            // ✅ LoRa 큐에 추가 (자동으로 메시지큐 신호 전송)
            if (!lora_queue_enqueue(&g_lora_queue, rtcm_data, rtcm_len)) {
                LOG_WARN("LoRa 큐 풀! 패킷 드롭 (메시지 타입: %d)",
                         rtcm_get_message_type(rtcm_data));
            } else {
                LOG_DEBUG("RTCM→LoRa 큐: %d bytes, 타입=%d",
                         rtcm_len, rtcm_get_message_type(rtcm_data));
            }
        }
    }
}

/**
 * @brief LoRa HAL 전송 함수 (사용자가 구현해야 함)
 *
 * @param data 전송할 데이터
 * @param len 데이터 길이
 * @return 0: 성공, <0: 실패
 */
int lora_hal_send(const uint8_t *data, uint8_t len) {
    // TODO: 실제 LoRa 모듈 전송 코드 구현
    // 예: SPI, UART 등으로 LoRa 모듈에 데이터 전송

    // 예시 (SPI 사용):
    // HAL_SPI_Transmit(&hspi1, (uint8_t*)data, len, 1000);

    return 0;  // 성공
}
