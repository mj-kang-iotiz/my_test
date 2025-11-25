#ifndef GPS_H
#define GPS_H

#ifndef TAG
	#define TAG "GPS"
#endif

#include "gps_nmea.h"
#include "gps_ubx.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define GPS_PAYLOAD_SIZE 256

typedef struct {
  int (*init)(void);
  int (*reset)(void);
  int (*send)(const char *data, size_t len);
  int (*recv)(char *buf, size_t len);
} gps_hal_ops_t;

typedef void (*evt_handler)(gps_t* gps, gps_procotol_t protocol, gps_msg_t msg);

/**
 * @brief GPS 구조체
 *
 */
typedef struct gps_s {
  /* state */
  gps_procotol_t protocol;

  /* os variable */
  SemaphoreHandle_t mutex;

  /* hal */
  const gps_hal_ops_t* ops;

  /* parse */
  gps_parse_state_t state;
  char payload[GPS_PAYLOAD_SIZE];
  uint32_t pos;

  /* protocol header */
  gps_nmea_parser_t nmea;
  gps_ubx_parser_t ubx;

  /* info */
  gps_nmea_data_t nmea_data;
  gps_ubx_data_t ubx_data;

  /* evt handler */
  evt_handler handler;
} gps_t;

void gps_init(gps_t *gps);
void gps_parse_process(gps_t *gps, const void *data, size_t len);
void gps_set_evt_handler(gps_t *gps, evt_handler handler);

bool get_gga(gps_t *gps, char* buf, uint8_t* len);

/* internal */
void _gps_gga_raw_add(gps_t *gps, char ch);

#endif
