#ifndef HELLO_WORLD_TOUCH_HELPER_H
#define HELLO_WORLD_TOUCH_HELPER_H
#include <stdbool.h>

/**
 * @brief Initializes the touch sensor subsystem.
 *
 * This function sets up the ESP32 touch sensors for the "next" and "previous"
 * touch buttons, configuring thresholds.
 */
void init_touches();

/**
 * @brief Updates the state of all touch sensors.
 *
 * This function should be called regularly to read the current touch sensor
 * values and update the internal state for press, hold, and release detection.
 */
void update_touches();

/**
 * @brief Checks if the "next" touch button was pressed.
 *
 * This function returns true if the "next" touch button has been newly pressed
 * since the last update.
 *
 * @return true if the "next" button was pressed, false otherwise.
 */
bool is_next_pressed();

/**
 * @brief Checks if the "previous" touch button was pressed.
 *
 * This function returns true if the "previous" touch button has been newly pressed
 * since the last update.
 *
 * @return true if the "previous" button was pressed, false otherwise.
 */
bool is_prev_pressed();

/**
 * @brief Checks if the "next" touch button is being held.
 *
 * This function returns true if the "next" touch button is currently being held
 * (continuously touched) beyond the initial press threshold.
 *
 * @return true if the "next" button is being held, false otherwise.
 */
bool is_next_hold();

/**
 * @brief Checks if the "previous" touch button is being held.
 *
 * This function returns true if the "previous" touch button is currently being held
 * (continuously touched) beyond the initial press threshold.
 *
 * @return true if the "previous" button is being held, false otherwise.
 */
bool is_prev_hold();

/**
 * @brief Checks if the "next" touch button was released.
 *
 * This function returns true if the "next" touch button has been released
 * since the last update.
 *
 * @return true if the "next" button was released, false otherwise.
 */
bool is_next_released();

/**
 * @brief Checks if the "previous" touch button was released.
 *
 * This function returns true if the "previous" touch button has been released
 * since the last update.
 *
 * @return true if the "previous" button was released, false otherwise.
 */
bool is_prev_released();

#endif //HELLO_WORLD_TOUCH_HELPER_H
