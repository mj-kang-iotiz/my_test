// GPS fix 상태 주기적 전송 예제 (10초마다)
#include "lora_queue.h"
#include "gps.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

// 전역 LoRa 큐
extern lora_queue_t g_lora_queue;

/**
 * @brief GPS fix 상태를 LoRa 큐에 추가
 */
static void send_gps_status_to_lora(gps_t *gps) {
    if (!gps) return;

    lora_gps_status_t status;

    // GPS fix 상태 채우기
    xSemaphoreTake(gps->mutex, portMAX_DELAY);

    status.fix_type = gps->nmea_data.gga.fix;  // GPS fix 타입
    status.num_satellites = gps->nmea_data.gga.sats;  // 위성 개수

    // HDOP (100배 스케일)
    status.hdop = (uint16_t)(gps->nmea_data.gga.hdop * 100);

    // 위도/경도/고도 (정수 변환)
    status.latitude = (int32_t)(gps->nmea_data.gga.lat * 1e7);
    status.longitude = (int32_t)(gps->nmea_data.gga.lon * 1e7);
    status.altitude = (int32_t)(gps->nmea_data.gga.alt * 1000);

    xSemaphoreGive(gps->mutex);

    // LoRa 큐에 추가
    if (!lora_queue_enqueue_status(&g_lora_queue, &status)) {
        LOG_WARN("GPS 상태 패킷 드롭 (LoRa 큐 풀)");
    } else {
        LOG_DEBUG("GPS 상태 전송: fix=%d, sats=%d, lat=%.7f, lon=%.7f",
                 status.fix_type, status.num_satellites,
                 gps->nmea_data.gga.lat, gps->nmea_data.gga.lon);
    }
}

/**
 * @brief 10초 타이머 콜백
 */
static void gps_status_timer_callback(TimerHandle_t xTimer) {
    gps_t *gps = (gps_t *)pvTimerGetTimerID(xTimer);
    send_gps_status_to_lora(gps);
}

/**
 * @brief GPS 상태 주기 전송 타이머 시작
 * @param gps GPS 핸들
 */
void lora_start_gps_status_timer(gps_t *gps) {
    if (!gps) return;

    // 10초 주기 타이머 생성 (auto-reload)
    TimerHandle_t timer = xTimerCreate(
        "gps_status",                    // 타이머 이름
        pdMS_TO_TICKS(10000),           // 10초 주기
        pdTRUE,                         // auto-reload
        (void *)gps,                    // 타이머 ID (GPS 핸들 전달)
        gps_status_timer_callback       // 콜백 함수
    );

    if (timer) {
        xTimerStart(timer, 0);
        LOG_INFO("GPS 상태 주기 전송 시작 (10초 간격)");
    } else {
        LOG_ERR("GPS 상태 타이머 생성 실패");
    }
}

/**
 * @brief LoRa 수신 측: GPS 상태 패킷 파싱
 */
void lora_parse_gps_status(const uint8_t *data, uint8_t len) {
    if (!data || len < sizeof(lora_gps_status_t)) {
        LOG_ERR("GPS 상태 패킷 길이 오류: %d", len);
        return;
    }

    lora_gps_status_t status;
    memcpy(&status, data, sizeof(lora_gps_status_t));

    // 역변환
    double lat = status.latitude / 1e7;
    double lon = status.longitude / 1e7;
    double alt = status.altitude / 1000.0;
    double hdop = status.hdop / 100.0;

    LOG_INFO("GPS 상태 수신:");
    LOG_INFO("  Fix: %d, Sats: %d, HDOP: %.2f",
             status.fix_type, status.num_satellites, hdop);
    LOG_INFO("  Pos: %.7f, %.7f, %.3fm", lat, lon, alt);

    // 여기서 UI 업데이트나 추가 처리...
}

/**
 * @brief LoRa 수신 핸들러 (Rover 측)
 */
void lora_receive_handler_with_status(uint8_t *data, uint8_t len) {
    if (len < LORA_CHUNK_HEADER_SIZE) return;

    // 헤더 파싱
    uint8_t packet_type = data[0];
    uint8_t packet_id = data[1];
    uint8_t chunk_index = data[2];
    uint8_t total_chunks = data[3];

    if (packet_type == LORA_PACKET_TYPE_RTCM) {
        // RTCM 패킷 재조립
        LOG_DEBUG("RTCM 청크 수신: Pkt=%d, Chunk=%d/%d",
                 packet_id, chunk_index + 1, total_chunks);

        // 기존 RTCM 재조립 로직...
        // (RTCM_LORA_USAGE.md 참고)

    } else if (packet_type == LORA_PACKET_TYPE_STATUS) {
        // GPS 상태 패킷 (청킹 없음, 1개 청크)
        LOG_DEBUG("GPS 상태 패킷 수신: Pkt=%d", packet_id);

        uint8_t *payload = &data[LORA_CHUNK_HEADER_SIZE];
        uint8_t payload_len = len - LORA_CHUNK_HEADER_SIZE;

        lora_parse_gps_status(payload, payload_len);
    } else {
        LOG_WARN("알 수 없는 LoRa 패킷 타입: 0x%02X", packet_type);
    }
}
