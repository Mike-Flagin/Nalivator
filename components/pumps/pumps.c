#include "pumps.h"

#include <string.h>
#include <driver/gpio.h>

#include "esp_system.h"
#include "../../include/pins.h"
#include "spi_helper.h"

#define bit_set(value, bit) ((value) |= (1UL << (bit)))
#define bit_clear(value, bit) ((value) &= ~(1UL << (bit)))
#define bit_write(value, bit, bit_value) ((bit_value) ? bit_set(value, bit) : bit_clear(value, bit))

pump_t pumps[PUMPS_AMOUNT];

static uint16_t pumps_shift_register_value;

void init_pumps()
{
    ESP_ERROR_CHECK(spi_init());
    for (int i = 0; i < PUMPS_AMOUNT; i++)
    {
        gpio_num_t pin;
        switch (i)
        {
        case 0:
            pin = PUMP_1_PIN;
            break;
        case 1:
            pin = PUMP_2_PIN;
            break;
        case 2:
            pin = PUMP_3_PIN;
            break;
        case 3:
            pin = PUMP_4_PIN;
            break;
        case 4:
            pin = PUMP_5_PIN;
            break;
        case 5:
            pin = PUMP_6_PIN;
            break;
        case 6:
            pin = PUMP_7_PIN;
            break;
        case 7:
            pin = PUMP_8_PIN;
            break;
        case 8:
            pin = PUMP_9_PIN;
            break;
        case 9:
            pin = PUMP_10_PIN;
            break;
        default:
            pin = GPIO_NUM_NC;
        }
        pumps[i].number = i;
        pumps[i].pin = pin;;
    }
    const gpio_config_t cfg = {
        .pin_bit_mask = 1 << PUMP_1_PIN | 1 << PUMP_2_PIN | 1 << PUMP_3_PIN | 1 << PUMP_4_PIN | 1ULL << PUMP_5_PIN
        | 1 << PUMP_6_PIN | 1 << PUMP_7_PIN | 1 << PUMP_8_PIN | 1 << PUMP_9_PIN | 1 << PUMP_10_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));


}

void configure_pumps(const uint16_t* flowrates, const bool* inverses, const uint16_t* ingredients, const uint8_t* volumes_to_splitter)
{
    for (uint8_t i = 0; i < PUMPS_AMOUNT; i++)
    {
        pumps[i].flowrate = flowrates[i];
        pumps[i].inverse = inverses[i];
        pumps[i].ingredient_id = ingredients[i];
        pumps[i].volume_to_splitter = volumes_to_splitter[i];
    }
}

void enable_pump(const pump_t* pump, direction_t direction)
{
    if (pump->inverse)
        direction = -direction;
    bit_write(pumps_shift_register_value, pump->number, direction != FORWARD);
    ESP_ERROR_CHECK(spi_write(pumps_shift_register_value));
    gpio_set_level(pump->pin, direction == FORWARD);
}

void disable_pump(const pump_t* pump)
{
    bit_set(pumps_shift_register_value, pump->number);
    ESP_ERROR_CHECK(spi_write(pumps_shift_register_value));
    gpio_set_level(pump->pin, 1);
}

void disable_all_pumps()
{
    pumps_shift_register_value = 0b1111111111111111; // set all to 1
    ESP_ERROR_CHECK(spi_write(pumps_shift_register_value));
    for (uint8_t i = 0; i < PUMPS_AMOUNT; i++)
    {
        gpio_set_level(pumps[i].pin, 1);
    }
}
