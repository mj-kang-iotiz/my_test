#include "gps_app.h"
#include "gps.h"
#include "gps_port.h"
#include "led.h"

static gps_instance_t* gps_instances[GPS_ID_MAX] = {NULL};
static gps_instance_t gps_base_handle;
static gps_instance_t gps_rover_handle;

char gps_recv[2048];
gps_t gps_handle;
QueueHandle_t gps_queue;

gps_t* gps_get_handle()
{
    return &gps_handle;
}

static void gps_process_task(void *pvParameter);

/**
 * @brief GPS 태스크 생성
 *
 * @param arg
 */
void gps_task_create(void *arg) {
  xTaskCreate(gps_process_task, "gps", 2048, arg, tskIDLE_PRIORITY + 1, NULL);
}

/**
 * @brief GPS 태스크
 *
 * @param pvParameter
 */

 char my_test[100];
uint8_t my_len = 0;

static void gps_process_task(void *pvParameter) {
  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;
  size_t total_received = 0;

  gps_queue = xQueueCreate(10, 1);

  gps_init(&gps_handle);
  gps_port_init();
  gps_start();

  led_set_color(2, LED_COLOR_RED);
  led_set_state(2, true);

  while (1) {
    xQueueReceive(gps_queue, &dummy, portMAX_DELAY);

    if(gps_handle.nmea_data.gga.fix == GPS_FIX_INVALID)
    {
    	led_set_color(2, LED_COLOR_RED);
    }
    else if(gps_handle.nmea_data.gga.fix < GPS_FIX_RTK_FIX)
    {
    	led_set_color(2, LED_COLOR_YELLOW);
    }
    else if(gps_handle.nmea_data.gga.fix < GPS_FIX_RTK_FLOAT)
    {
    	led_set_color(2, LED_COLOR_GREEN);
    }
    else
    {
    	led_set_color(2, LED_COLOR_NONE);
    }

    led_set_toggle(2);

    xSemaphoreTake(gps_handle.mutex, portMAX_DELAY);
    pos = gps_get_rx_pos();

    if (pos != old_pos) {
      if (pos > old_pos) {
        size_t len = pos - old_pos;
        total_received = len;
        LOG_DEBUG("RX: %u bytes", len);
        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len);
        gps_parse_process(&gps_handle, &gps_recv[old_pos], pos - old_pos);
      } else {
        size_t len1 = sizeof(gps_recv) - old_pos;
        size_t len2 = pos;
        total_received = len1 + len2;
        LOG_DEBUG("RX: %u bytes (wrapped: %u+%u)", total_received, len1, len2);

        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len1);
        gps_parse_process(&gps_handle, &gps_recv[old_pos],
                          sizeof(gps_recv) - old_pos);
        if (pos > 0) {
          LOG_DEBUG_RAW("RAW: ", gps_recv, len2);
          gps_parse_process(&gps_handle, gps_recv, pos);
        }
      }
      old_pos = pos;
      if (old_pos == sizeof(gps_recv)) {
        old_pos = 0;
      }
    }
    xSemaphoreGive(gps_handle.mutex);

    if(get_gga(&gps_handle, my_test, &my_len))
    {
    	LOG_ERR("%s", my_test);
    }
  }

  vTaskDelete(NULL);
}
