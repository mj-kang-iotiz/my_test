#include "gps_app.h"
#include "gps.h"
#include "gps_port.h"
#include "board_config.h"
#include "led.h"
#include <string.h>

#define GGA_AVG_SIZE 100

// GPS 초기화 상태
typedef enum {
  GPS_INIT_STATE_WAIT_READY,    // UM982: RDY 대기
  GPS_INIT_STATE_SEND_CONFIG,   // 설정 명령 전송 중
  GPS_INIT_STATE_WAIT_ACK,      // 설정 응답 대기
  GPS_INIT_STATE_READY          // 정상 동작 중
} gps_init_state_t;

typedef struct {
  gps_t handle;
  QueueHandle_t queue;
  TaskHandle_t task;
  gps_type_t type;
  bool enabled;
  gps_init_state_t init_state;

  struct {
    double lat[GGA_AVG_SIZE];
    double lon[GGA_AVG_SIZE];
    double alt[GGA_AVG_SIZE];
    double lat_avg;
    double lon_avg;
    double alt_avg;
    uint8_t pos;
    uint8_t len;
    bool can_read;
  } gga_avg_data;
} gps_instance_t;

static gps_instance_t gps_instances[GPS_ID_MAX] = {0};

// Legacy support
static gps_instance_t* gps_legacy_handle = NULL;

char my_test[100];
uint8_t my_len = 0;

static void _add_gga_avg_data(gps_instance_t* inst, double lat, double lon, double alt)
{
  uint8_t pos = inst->gga_avg_data.pos;
  double lat_temp = 0.0, lon_temp = 0.0, alt_temp = 0.0;

  double prev_lat = inst->gga_avg_data.lat[pos];
  double prev_lon = inst->gga_avg_data.lon[pos];
  double prev_alt = inst->gga_avg_data.alt[pos];

  inst->gga_avg_data.lat[pos] = lat;
  inst->gga_avg_data.lon[pos] = lon;
  inst->gga_avg_data.alt[pos] = alt;

  inst->gga_avg_data.pos = (inst->gga_avg_data.pos + 1) % GGA_AVG_SIZE;

  /* 정확도를 중시한 코드 */
  if(inst->gga_avg_data.len < GGA_AVG_SIZE)
  {
    inst->gga_avg_data.len++;

    if(inst->gga_avg_data.len == GGA_AVG_SIZE)
    {

      for(int i = 0; i < GGA_AVG_SIZE; i++)
      {
        lat_temp += inst->gga_avg_data.lat[i];
        lon_temp += inst->gga_avg_data.lon[i];
        alt_temp += inst->gga_avg_data.alt[i];
      }

      inst->gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
      inst->gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
      inst->gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;

      inst->gga_avg_data.can_read = true;
    }
  }
  else
  {
      for(int i = 0; i < GGA_AVG_SIZE; i++)
      {
        lat_temp += inst->gga_avg_data.lat[i];
        lon_temp += inst->gga_avg_data.lon[i];
        alt_temp += inst->gga_avg_data.alt[i];
      }

      inst->gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
      inst->gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
      inst->gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;
  }
}

/**
 * @brief GPS 타입별 설정 명령 전송
 */
static void gps_send_config_commands(gps_instance_t* inst, gps_id_t id) {
  if (!inst->handle.ops || !inst->handle.ops->send) return;

  if (inst->type == GPS_TYPE_UM982) {
    // Unicore UM982 설정 명령
    LOG_INFO("GPS[%d] Sending UM982 config commands", id);

    // TODO: 실제 UM982 명령어로 교체 필요
    // 예: "CONFIG xxx" 또는 "GNGGA 1" 등
    const char* cmd1 = "GNGGA 1\r\n";  // GGA 메시지 1초 간격 출력
    inst->handle.ops->send(cmd1, strlen(cmd1));

    vTaskDelay(pdMS_TO_TICKS(100));

    // 추가 설정 명령이 필요하면 여기에

    // ACK 대기 상태로 전환
    inst->init_state = GPS_INIT_STATE_WAIT_ACK;
    LOG_INFO("GPS[%d] Waiting for UM982 ACK", id);

  } else if (inst->type == GPS_TYPE_F9P) {
    // Ublox F9P 설정 명령
    LOG_INFO("GPS[%d] Sending F9P config commands", id);

    // F9P는 보통 기본 설정으로 동작하거나 UBX 명령 사용
    // 필요시 $PUBX 또는 UBX 바이너리 명령 추가

    // F9P는 설정 없이 바로 동작 (또는 ACK 대기)
    inst->init_state = GPS_INIT_STATE_READY;
    LOG_INFO("GPS[%d] F9P ready (no ACK needed)", id);
  }
}

/**
 * @brief 수신 데이터에서 "RDY" 문자열 검색
 */
static bool check_for_rdy(const char* data, size_t len) {
  if (len < 3) return false;

  for (size_t i = 0; i <= len - 3; i++) {
    if (data[i] == 'R' && data[i+1] == 'D' && data[i+2] == 'Y') {
      return true;
    }
  }
  return false;
}

/**
 * @brief 수신 데이터에서 "OK" 응답 검색 (UM982)
 */
static bool check_for_ok(const char* data, size_t len) {
  if (len < 2) return false;

  for (size_t i = 0; i <= len - 2; i++) {
    if (data[i] == 'O' && data[i+1] == 'K') {
      return true;
    }
  }
  return false;
}

/**
 * @brief F9P ACK 확인 (간단 버전 - UBX-ACK 또는 즉시 OK)
 */
static bool check_f9p_ack(const char* data, size_t len) {
  // F9P는 설정 없이 바로 동작하거나, UBX-ACK 확인 필요
  // 간단하게 일정 시간 후 OK로 간주
  // TODO: 실제 UBX-ACK-ACK (0xB5 0x62 0x05 0x01) 파싱 필요
  (void)data;
  (void)len;
  return true;  // 임시: 바로 OK
}

void gps_evt_handler(gps_t* gps, gps_procotol_t protocol, uint8_t msg)
{
  // Find which instance this GPS belongs to
  gps_instance_t* inst = NULL;
  for (uint8_t i = 0; i < GPS_ID_MAX; i++) {
    if (gps_instances[i].enabled && &gps_instances[i].handle == gps) {
      inst = &gps_instances[i];
      break;
    }
  }

  if (!inst) return;

  switch(protocol)
  {
    case GPS_PROTOCOL_NMEA:
      if(msg == GPS_NMEA_MSG_GGA)
      {
        if(gps->nmea_data.gga.fix == GPS_FIX_GPS)
        {
          _add_gga_avg_data(inst, gps->nmea_data.gga.lat, gps->nmea_data.gga.lon, gps->nmea_data.gga.alt);
        }
      }
      break;
  }
}

static void gps_process_task(void *pvParameter)
{
  gps_id_t id = (gps_id_t)(uintptr_t)pvParameter;
  gps_instance_t* inst = &gps_instances[id];

  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;
  size_t total_received = 0;

  gps_set_evt_handler(&inst->handle, gps_evt_handler);
  memset(&inst->gga_avg_data, 0, sizeof(inst->gga_avg_data));

  // GPS별로 다른 LED 할당 (첫 번째: LED_ID_2, 두 번째: LED_ID_3)
  uint8_t led_id = (id == GPS_ID_BASE) ? LED_ID_2 : LED_ID_3;

  led_set_color(led_id, LED_COLOR_RED);
  led_set_state(led_id, true);

  // F9P는 부팅 대기 후 바로 설정 전송
  if (inst->type == GPS_TYPE_F9P && inst->init_state == GPS_INIT_STATE_SEND_CONFIG) {
    vTaskDelay(pdMS_TO_TICKS(500));  // F9P 부팅 대기
    gps_send_config_commands(inst, id);
  }

  while (1) {
    xQueueReceive(inst->queue, &dummy, portMAX_DELAY);

    // LED status based on GPS fix quality
    if(inst->handle.nmea_data.gga.fix == GPS_FIX_INVALID)
    {
      led_set_color(led_id, LED_COLOR_RED);
    }
    else if(inst->handle.nmea_data.gga.fix < GPS_FIX_RTK_FIX)
    {
      led_set_color(led_id, LED_COLOR_YELLOW);
    }
    else if(inst->handle.nmea_data.gga.fix >= GPS_FIX_RTK_FLOAT)
    {
      led_set_color(led_id, LED_COLOR_GREEN);
    }
    else
    {
      led_set_color(led_id, LED_COLOR_NONE);
    }

    led_set_toggle(led_id);

    xSemaphoreTake(inst->handle.mutex, portMAX_DELAY);
    pos = gps_port_get_rx_pos(id);
    char* gps_recv = gps_port_get_recv_buf(id);
    size_t gps_recv_size = 2048;  // From gps_port.c buffer size

    if (pos != old_pos) {
      if (pos > old_pos) {
        size_t len = pos - old_pos;
        total_received = len;
        LOG_DEBUG("GPS[%d] RX: %u bytes", id, len);
        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len);

        // 초기화 상태별 처리
        if (inst->init_state == GPS_INIT_STATE_WAIT_READY) {
          // UM982: RDY 대기 중
          if (check_for_rdy(&gps_recv[old_pos], len)) {
            LOG_INFO("GPS[%d] RDY received, sending configuration", id);
            gps_send_config_commands(inst, id);  // 내부에서 WAIT_ACK로 변경
          }
        } else if (inst->init_state == GPS_INIT_STATE_WAIT_ACK) {
          // 설정 명령 ACK 대기 중
          bool ack_received = false;

          if (inst->type == GPS_TYPE_UM982) {
            ack_received = check_for_ok(&gps_recv[old_pos], len);
          } else if (inst->type == GPS_TYPE_F9P) {
            ack_received = check_f9p_ack(&gps_recv[old_pos], len);
          }

          if (ack_received) {
            LOG_INFO("GPS[%d] ACK received, configuration complete", id);
            inst->init_state = GPS_INIT_STATE_READY;
          }
        } else if (inst->init_state == GPS_INIT_STATE_READY) {
          // 정상 동작: 데이터 파싱
          gps_parse_process(&inst->handle, &gps_recv[old_pos], pos - old_pos);
        }

      } else {
        size_t len1 = gps_recv_size - old_pos;
        size_t len2 = pos;
        total_received = len1 + len2;
        LOG_DEBUG("GPS[%d] RX: %u bytes (wrapped: %u+%u)", id, total_received, len1, len2);

        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len1);

        // 초기화 상태별 처리 (버퍼 랩어라운드)
        if (inst->init_state == GPS_INIT_STATE_WAIT_READY) {
          // UM982: RDY 대기 중
          if (check_for_rdy(&gps_recv[old_pos], len1) ||
              (pos > 0 && check_for_rdy(gps_recv, len2))) {
            LOG_INFO("GPS[%d] RDY received, sending configuration", id);
            gps_send_config_commands(inst, id);  // 내부에서 WAIT_ACK로 변경
          }
        } else if (inst->init_state == GPS_INIT_STATE_WAIT_ACK) {
          // 설정 명령 ACK 대기 중
          bool ack_received = false;

          if (inst->type == GPS_TYPE_UM982) {
            ack_received = check_for_ok(&gps_recv[old_pos], len1) ||
                          (pos > 0 && check_for_ok(gps_recv, len2));
          } else if (inst->type == GPS_TYPE_F9P) {
            ack_received = check_f9p_ack(&gps_recv[old_pos], len1);
          }

          if (ack_received) {
            LOG_INFO("GPS[%d] ACK received, configuration complete", id);
            inst->init_state = GPS_INIT_STATE_READY;
          }
        } else if (inst->init_state == GPS_INIT_STATE_READY) {
          // 정상 동작: 데이터 파싱
          gps_parse_process(&inst->handle, &gps_recv[old_pos],
                            gps_recv_size - old_pos);
          if (pos > 0) {
            LOG_DEBUG_RAW("RAW: ", gps_recv, len2);
            gps_parse_process(&inst->handle, gps_recv, pos);
          }
        }
      }
      old_pos = pos;
      if (old_pos == gps_recv_size) {
        old_pos = 0;
      }
    }
    xSemaphoreGive(inst->handle.mutex);

    if(get_gga(&inst->handle, my_test, &my_len))
    {
      LOG_ERR("GPS[%d] GGA: %s", id, my_test);
    }
  }

  vTaskDelete(NULL);
}

/**
 * @brief GPS 태스크 생성 (레거시 호환용)
 *
 * main.c에서 gps_task_create(NULL) 호출 시 사용
 */
void gps_task_create(void *arg) {
  (void)arg;  // 사용하지 않음
  gps_init_all();
}

/**
 * @brief GPS 전체 초기화 (보드 설정 기반)
 */
void gps_init_all(void)
{
  const board_config_t* config = board_get_config();

  LOG_INFO("GPS 초기화 시작 - 보드: %d, GPS 개수: %d", config->board, config->gps_cnt);

  for (uint8_t i = 0; i < config->gps_cnt && i < GPS_ID_MAX; i++) {
    gps_type_t type = config->gps[i];

    LOG_INFO("GPS[%d] 초기화 - 타입: %s", i,
             type == GPS_TYPE_F9P ? "F9P" :
             type == GPS_TYPE_UM982 ? "UM982" : "UNKNOWN");

    // GPS 핸들 초기화
    gps_init(&gps_instances[i].handle);
    gps_instances[i].type = type;
    gps_instances[i].enabled = true;

    // GPS 타입별 초기 상태 설정
    if (type == GPS_TYPE_UM982) {
      // Unicore UM982: RDY 대기
      gps_instances[i].init_state = GPS_INIT_STATE_WAIT_READY;
      LOG_INFO("GPS[%d] UM982 will wait for RDY signal", i);
    } else if (type == GPS_TYPE_F9P) {
      // Ublox F9P: 바로 설정 전송 (또는 바로 READY)
      gps_instances[i].init_state = GPS_INIT_STATE_SEND_CONFIG;
      LOG_INFO("GPS[%d] F9P will send config immediately", i);
    } else {
      gps_instances[i].init_state = GPS_INIT_STATE_READY;
    }

    // 포트 초기화 (UART 설정 및 HAL ops 자동 할당)
    if (gps_port_init_instance(&gps_instances[i].handle, (gps_id_t)i, type) != 0) {
      LOG_ERR("GPS[%d] 포트 초기화 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    // 큐 생성 및 설정
    gps_instances[i].queue = xQueueCreate(10, sizeof(uint8_t));
    if (gps_instances[i].queue == NULL) {
      LOG_ERR("GPS[%d] 큐 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    gps_port_set_queue((gps_id_t)i, gps_instances[i].queue);

    // UART 시작
    gps_port_start((gps_id_t)i);

    // 태스크 생성
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "gps_%d", i);

    BaseType_t ret = xTaskCreate(
      gps_process_task,
      task_name,
      2048,
      (void*)(uintptr_t)i,  // GPS ID를 파라미터로 전달
      tskIDLE_PRIORITY + 1,
      &gps_instances[i].task
    );

    if (ret != pdPASS) {
      LOG_ERR("GPS[%d] 태스크 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    // 첫 번째 GPS를 레거시 핸들로 설정 (하위 호환성)
    if (gps_legacy_handle == NULL) {
      gps_legacy_handle = &gps_instances[i];
    }

    LOG_INFO("GPS[%d] 초기화 완료", i);
  }

  LOG_INFO("GPS 전체 초기화 완료");
}

/**
 * @brief GPS 핸들 가져오기 (레거시 호환용 - 첫 번째 GPS 반환)
 */
gps_t* gps_get_handle(void)
{
  if (gps_legacy_handle != NULL) {
    return &gps_legacy_handle->handle;
  }

  // Fallback: 첫 번째 활성화된 GPS 찾기
  for (uint8_t i = 0; i < GPS_ID_MAX; i++) {
    if (gps_instances[i].enabled) {
      gps_legacy_handle = &gps_instances[i];
      return &gps_instances[i].handle;
    }
  }

  return NULL;
}

/**
 * @brief 특정 GPS ID의 핸들 가져오기
 */
gps_t* gps_get_instance_handle(gps_id_t id)
{
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    return NULL;
  }

  return &gps_instances[id].handle;
}

/**
 * @brief GGA 평균 데이터 읽기 가능 여부
 */
bool gps_gga_avg_can_read(gps_id_t id)
{
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    return false;
  }

  return gps_instances[id].gga_avg_data.can_read;
}

/**
 * @brief GGA 평균 데이터 가져오기
 */
bool gps_get_gga_avg(gps_id_t id, double* lat, double* lon, double* alt)
{
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    return false;
  }

  if (!gps_instances[id].gga_avg_data.can_read) {
    return false;
  }

  if (lat) *lat = gps_instances[id].gga_avg_data.lat_avg;
  if (lon) *lon = gps_instances[id].gga_avg_data.lon_avg;
  if (alt) *alt = gps_instances[id].gga_avg_data.alt_avg;

  return true;
}
