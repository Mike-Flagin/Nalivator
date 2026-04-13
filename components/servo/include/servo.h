//
// Created by mikes on 07.04.2026.
//

#ifndef NALIVATOR_SERVO_H
#define NALIVATOR_SERVO_H

#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0

#define LEDC_MIN_DUTY_CYCLE ((1<<20) * 0.5 / 20)
#define LEDC_MAX_DUTY_CYCLE ((1<<20) * 2.5 / 20)
#include <stdint.h>

void init_servo();
void set_servo_position(uint16_t angle);

#endif //NALIVATOR_SERVO_H
