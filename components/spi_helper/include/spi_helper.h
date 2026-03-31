#ifndef NALIVATOR_SPI_HELPER_H
#define NALIVATOR_SPI_HELPER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initializes the SPI interface for communication.
 *
 * This function sets up the SPI bus and device configuration for
 * communicating with shift registers.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t spi_init(void);

/**
 * @brief Writes 16-bit data to the SPI device.
 *
 * This function transmits the provided 16-bit data word to the shift registers.
 *
 * @param data The 16-bit data to write to the shift registers.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t spi_write(uint16_t data);

#endif
