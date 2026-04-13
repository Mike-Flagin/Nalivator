//
// Created by mikes on 07.04.2026.
//

#include "servo.h"

#include "driver/ledc.h"
#include "../../include/pins.h"

void init_servo()
{
    // Prepare and then apply the LEDC PWM timer configuration
    const ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_TIMER_20_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    const ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = SERVO_PIN,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void set_servo_position(uint16_t angle)
{
    if (angle > 270) angle = 270;
    //convert value from 0-270 to min-max duty
    const uint32_t duty = (uint32_t)angle * (LEDC_MAX_DUTY_CYCLE - LEDC_MIN_DUTY_CYCLE) / 270 + LEDC_MIN_DUTY_CYCLE; // NOLINT(*-narrowing-conversions)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}
