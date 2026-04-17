//
// Created by mikes on 04.04.2026.
//
#ifndef NALIVATOR_TASKS_H
#define NALIVATOR_TASKS_H

#include "leds.h"
#include "pumps.h"
#include "recipe.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "../../include/values.h"
#include "../../include/files.h"

#define SIGNAL_ABORT      BIT0
#define SIGNAL_SERVO_DONE BIT1

extern TaskHandle_t pour_task_handle;
extern TaskHandle_t servo_task_handle;

extern SemaphoreHandle_t recipes_mutex;
extern SemaphoreHandle_t ingredients_mutex;
extern SemaphoreHandle_t config_mutex;

enum glass_state
{
    NOT_PRESENT,
    WAITING,
    POURING,
    POURED
};

typedef struct
{
    pump_t* pumps;
    uint16_t servo_positions[GLASSES_AMOUNT];
    uint8_t volume_after_splitter;
    color_t led_standby;
    color_t led_waiting;
    color_t led_pouring;
    color_t led_poured;
    recipe_t current_recipe;
    float portion;
} config_t;

extern config_t config;

void print_system_stats(void *pvParameters);
void init_tasks();
void rebuild_index_task(void *pvParameters);
void restart_esp(void* arg);
void load_config_task(void *pvParameters);
void button_handler_task(void *pvParameters);
void servo_task(void *pvParameters);
void leds_task(void *pvParameters);
void pour_task(void *pvParameters);

#endif //NALIVATOR_TASKS_H
