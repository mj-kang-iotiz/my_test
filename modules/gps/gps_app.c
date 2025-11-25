#include "gps_app.h"
#include "gps.h"
#include "gps_port.h"
#include "led.h"
#include <string.h>

static gps_instance_t* gps_instances[GPS_ID_MAX] = {NULL};
static gps_instance_t gps_base_handle;
static gps_instance_t gps_rover_handle;

char gps_recv[2048];
gps_t gps_handle;
QueueHandle_t gps_queue;

#define GGA_AVG_SIZE 100

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
}gga_avg_data;

void _add_gga_avg_data(double lat, double lon, double alt)
{
  uint8_t pos = gga_avg_data.pos;
  double lat_temp = 0.0, lon_temp = 0.0, alt_temp = 0.0;

  double prev_lat = gga_avg_data.lat[pos];
  double prev_lon = gga_avg_data.lon[pos];
  double prev_alt = gga_avg_data.alt[pos];

  gga_avg_data.lat[pos] = lat;
  gga_avg_data.lon[pos] = lon;
  gga_avg_data.alt[pos] = alt;

  gga_avg_data.pos = (gga_avg_data.pos + 1) % GGA_AVG_SIZE;

  /* 정확도를 중시한 코드 */
  if(gga_avg_data.len < GGA_AVG_SIZE)
  {
    gga_avg_data.len++;

    if(gga_avg_data.len == GGA_AVG_SIZE)
    {
      
      for(int i = 0; i < GGA_AVG_SIZE; i++)
      {
        lat_temp += gga_avg_data.lat[i];
        lon_temp += gga_avg_data.lon[i];
        alt_temp += gga_avg_data.alt[i];
      }

      gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
      gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
      gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;

      gga_avg_data.can_read = true;
    }
  }
  else
  {
      for(int i = 0; i < GGA_AVG_SIZE; i++)
      {
        lat_temp += gga_avg_data.lat[i];
        lon_temp += gga_avg_data.lon[i];
        alt_temp += gga_avg_data.alt[i];
      }

      gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
      gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
      gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;
  }

  /* 속도를 중시한 코드 */
  // if(gga_avg_data.len < GGA_AVG_SIZE)
  // {
  //   gga_avg_data.len++;

  //   if(gga_avg_data.len == GGA_AVG_SIZE)
  //   {
  //     double lat_temp = 0.0, lon_temp = 0.0, alt_temp = 0.0;
  //     for(int i = 0; i < GGA_AVG_SIZE; i++)
  //     {
  //       lat_temp += gga_avg_data.lat[i];
  //       lon_temp += gga_avg_data.lon[i];
  //       alt_temp += gga_avg_data.alt[i];
  //     }

  //     gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
  //     gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
  //     gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;

  //     gga_avg_data.can_read = true;
  //   }
  // }
  // else
  // {
  //   gga_avg_data.lat_avg += ((lat - prev_lat) / (double)GGA_AVG_SIZE);
  //   gga_avg_data.lon_avg += ((lon - prev_lon) / (double)GGA_AVG_SIZE);
  //   gga_avg_data.alt_avg += ((alt - prev_alt) / (double)GGA_AVG_SIZE);
  // }
}

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

void gps_evt_handler(gps_t* gps, gps_procotol_t protocol, gps_msg_t msg)
{
  switch(protocol)
  {
    case GPS_PROTOCOL_NMEA:
      if(msg.nmea_msg == GPS_NMEA_MSG_GGA)
      {
        if(gps->nmea_data.gga.fix == GPS_FIX_GPS)
        {
          _add_gga_avg_data(gps->nmea_data.gga.lat, gps->nmea_data.gga.lon, gps->nmea_data.gga.alt);
        }
      }
      break;

    case GPS_PROTOCOL_UBX:
      if(msg.ubx.class == GPS_UBX_CLASS_NAV && msg.ubx.id == GPS_UBX_NAV_ID_HPPOSLLH)
      {
        // UBX NAV-HPPOSLLH 메시지 처리
        // TODO: UBX 데이터 처리 로직 추가
      }
      break;

    default:
      break;
  }
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

  gps_set_evt_handler(&gps_handle, gps_evt_handler);
  memset(&gga_avg_data, 0, sizeof(gga_avg_data));

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
