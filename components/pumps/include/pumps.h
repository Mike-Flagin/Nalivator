#ifndef HELLO_WORLD_PUMPS_H
#define HELLO_WORLD_PUMPS_H

#include <stdint.h>
#include "soc/gpio_num.h"
#include <stdbool.h>

#define PUMPS_AMOUNT 10

typedef enum {
	FORWARD = 1,
	OFF = 0,
 	BACKWARD = -1,
} direction_t;

typedef struct
{
    uint8_t number;
    gpio_num_t pin;
    uint16_t ingredient_id;
    uint16_t flowrate;
    bool inverse;
    uint8_t volume_to_splitter;
} pump_t;

// register - second value
// high high -- breaking
// high low -- forward
// low high -- backward
// low low -- open

/**
 * @brief Initializes the pumps subsystem, including SPI and GPIO configuration.
 *
 * This function sets up the SPI interface for communicating with the pump shift register,
 * initializes the pump array with default values, and configures the GPIO pins for pump control.
 */
void init_pumps();

/**
 * @brief Configures the pumps with specific parameters.
 *
 * @param flowrates Array of flowrates for each pump (size PUMPS_AMOUNT).
 * @param inverses Array of inverse flags for each pump (size PUMPS_AMOUNT).
 * @param ingredients
 * @param volumes_to_splitter Array of volumes to splitter for each pump (size PUMPS_AMOUNT).
 */
void configure_pumps(const uint16_t* flowrates, const bool* inverses, const uint16_t* ingredients, const uint8_t* volumes_to_splitter);

/**
 * @brief Enables a specific pump in the given direction.
 *
 * This function updates the shift register value based on the pump's inverse setting and direction,
 * writes the value to the SPI device, and sets the GPIO level accordingly.
 *
 * @param pump Pointer to the pump structure to enable.
 * @param direction The direction to enable the pump (FORWARD, BACKWARD, or OFF).
 */
void enable_pump(const pump_t* pump, direction_t direction);

/**
 * @brief Disables all pumps.
 *
 * This function sets all bits in the shift register to 1 (disable state),
 * writes the value to the SPI device, and sets all pump GPIO pins to high level.
 */
void disable_all_pumps();

/**
 * @brief Disables a specific pump.
 *
 * This function sets the corresponding bit in the shift register to 1 (disable state),
 * writes the value to the SPI device, and sets the pump's GPIO pin to high level.
 *
 * @param pump Pointer to the pump structure to disable.
 */
void disable_pump(const pump_t* pump);



#endif
