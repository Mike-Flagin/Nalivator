//
// Created by mikes on 04.04.2026.
//

#ifndef NALIVATOR_LEDS_H
#define NALIVATOR_LEDS_H

#include <stdint.h>

#include "../../../include/values.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000

typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

void init_leds(void);
void update_leds(color_t leds[GLASSES_AMOUNT]);

#endif //NALIVATOR_LED_H
