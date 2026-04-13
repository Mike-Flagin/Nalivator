//
// Created by mikes on 05.04.2026.
//

#ifndef NALIVATOR_BUTTONS_H
#define NALIVATOR_BUTTONS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t button_queue;

typedef struct {
    uint32_t pin_num;
    bool state;
} button_event_t;

void init_buttons();

#endif //NALIVATOR_LED_H
