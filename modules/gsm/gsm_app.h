#ifndef GSM_APP_H
#define GSM_APP_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

extern QueueHandle_t gsm_queue;

void gsm_task_create(void *arg);

#endif
