//
// Created by mikes on 05.04.2026.
//

#include "buttons.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include "esp_attr.h"
#include "portmacro.h"
#include "tasks.h"
#include "../../include/pins.h"

QueueHandle_t button_queue = NULL;

static void IRAM_ATTR button_isr_handler(void* arg)
{
    const uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    button_event_t event;
    event.pin_num = gpio_num;
    event.state = gpio_get_level(gpio_num);

    xQueueSendFromISR(button_queue, &event, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}


void init_buttons()
{
    button_queue = xQueueCreate(10, sizeof(uint32_t));

    static StaticTask_t buttons_task_buffer;
    static StackType_t buttons_stack[BUTTONS_TASK_STACK_SIZE];
    xTaskCreateStaticPinnedToCore(button_handler_task, "buttons_task", BUTTONS_TASK_STACK_SIZE,
                                                      NULL, 20,
                                                      buttons_stack, &buttons_task_buffer, 1);

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << KEY_1_PIN | 1ULL << KEY_2_PIN | 1ULL << KEY_3_PIN | 1ULL << KEY_4_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    // Install GPIO ISR service
    gpio_install_isr_service(0);

    // Add ISR handler for buttons
    gpio_isr_handler_add(KEY_1_PIN, button_isr_handler, (void*)KEY_1_PIN);
    gpio_isr_handler_add(KEY_2_PIN, button_isr_handler, (void*)KEY_2_PIN);
    gpio_isr_handler_add(KEY_3_PIN, button_isr_handler, (void*)KEY_3_PIN);
    gpio_isr_handler_add(KEY_4_PIN, button_isr_handler, (void*)KEY_4_PIN);
}
