//
// Created by mikes on 04.04.2026.
//
#ifndef NALIVATOR_TASKS_H
#define NALIVATOR_TASKS_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t recipes_mutex;
extern SemaphoreHandle_t ingredients_mutex;
extern SemaphoreHandle_t config_mutex;

void init_tasks();
void rebuild_index_task(void *pvParameters);
void restart_esp(void* arg);

#endif //NALIVATOR_TASKS_H
