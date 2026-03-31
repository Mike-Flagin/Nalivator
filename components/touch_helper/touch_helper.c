#include "touch_helper.h"

#include "driver/touch_pad.h"
#include "esp_timer.h"
#include "../../include/pins.h"
#include "../../include/values.h"

static bool touch_next_pressed = false;
static bool touch_next_hold = false;
static bool touch_prev_pressed = false;
static bool touch_prev_hold = false;
static int64_t touch_next_timer = 0;
static int64_t touch_prev_timer = 0;

void init_touches()
{
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V);
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_config(TOUCH_NEXT_PIN, TOUCH_THRESHOLD);
    touch_pad_config(TOUCH_PREV_PIN, TOUCH_THRESHOLD);
}

void update_touches()
{
    uint16_t touch_next_value;
    uint16_t touch_prev_value;
    touch_pad_read_filtered(TOUCH_NEXT_PIN, &touch_next_value);
    touch_pad_read_filtered(TOUCH_PREV_PIN, &touch_prev_value);

    const bool touch_next_state = touch_next_value < TOUCH_THRESHOLD;
    const bool touch_prev_state = touch_prev_value < TOUCH_THRESHOLD;

    const int64_t time = esp_timer_get_time();

    // pressed next
    if (touch_next_state && !touch_next_pressed)
    {
        touch_next_pressed = true;
        touch_next_timer = time;
    }
    // released next
    if (!touch_next_state && (touch_next_pressed || touch_next_hold))
    {
        touch_next_pressed = false;
        touch_next_hold = false;
    }
    // hold next
    if (touch_next_pressed && touch_next_state && time - touch_next_timer >= TOUCH_HOLD_TIMEOUT_MS * 1000)
    {
        touch_next_hold = true;
        touch_next_pressed = false;
    }

    // pressed prev
    if (touch_prev_state && !touch_prev_pressed)
    {
        touch_prev_pressed = true;
        touch_prev_timer = time;
    }
    // released prev
    if (!touch_prev_state && (touch_prev_pressed || touch_prev_hold))
    {
        touch_prev_pressed = false;
        touch_prev_hold = false;
    }
    // hold prev
    if (touch_prev_pressed && touch_prev_state && time - touch_prev_timer >= TOUCH_HOLD_TIMEOUT_MS * 1000)
    {
        touch_prev_hold = true;
        touch_prev_pressed = false;
    }
}

bool is_next_pressed()
{
    return touch_next_pressed;
}

bool is_prev_pressed()
{
    return touch_prev_pressed;
}

bool is_next_hold()
{
    return touch_next_hold;
}

bool is_prev_hold()
{
    return touch_prev_hold;
}

bool is_next_released()
{
    return !(touch_next_pressed || touch_next_hold);
}

bool is_prev_released()
{
    return !(touch_prev_pressed || touch_prev_hold);
}
