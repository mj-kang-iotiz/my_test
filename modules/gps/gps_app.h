#ifndef GPS_APP_H
#define GPS_APP_H

#include "gps.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

extern QueueHandle_t gps_queue;

/**
 * @brief GPS 인스턴스 ID
 */
typedef enum {
    GPS_ID_BASE = 0,    // 기준국 GPS
    GPS_ID_ROVER,       // 로버 GPS
    GPS_ID_MAX
} gps_id_t;

typedef struct gps_instance_s {
  gps_t handle;
  QueueHandle_t queue;
  TaskHandle_t task;
  SemaphoreHandle_t mutex;
  char *recv_buf;
  size_t recv_buf_size;
} gps_instance_t;

gps_t* gps_get_handle(void);
void gps_task_create(void *arg);

#endif
