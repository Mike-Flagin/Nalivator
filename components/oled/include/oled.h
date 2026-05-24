//
// Created by mikes on 08.05.2026.
//

#ifndef NALIVATOR_OLED_H
#define NALIVATOR_OLED_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_DISPLAY_ADDRESS 0x3C
#define I2C_FREQ_HZ 400000

#define RECIPES_UPDATED_BIT BIT0

#define DISPLAY_HEIGHT 64

#define LINE_HEIGHT 16 // Symbol Height + Line Offset After It
#define POINTER_RAD 3
#define LINES_PER_PAGE (DISPLAY_HEIGHT / LINE_HEIGHT)

extern TaskHandle_t display_task_handle;
extern EventGroupHandle_t display_events;

void init_oled(void);
void kill_display_task();

void oled_print_status(const char* text, bool animated);

//tasks
void show_status_animated(void* pvParameters);
void show_status_static(void* pvParameters);
void oled_print_qr(void* pvParameters);
void oled_menu_task(void* pvParameters);

#endif //NALIVATOR_OLED_H
