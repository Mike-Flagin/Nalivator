#include "spi_helper.h"
#include "driver/spi_master.h"
#include "../../include/pins.h"

static spi_device_handle_t spi_device;
static bool spi_initialized = false;

esp_err_t spi_init(void) {
    if (spi_initialized) return ESP_OK;

    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .miso_io_num = -1,  // No MISO
        .mosi_io_num = PUMPS_MOSI_PIN,
        .sclk_io_num = PUMPS_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2
    };

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        return ret;
    }

    // SPI device configuration
    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_MASTER_FREQ_16M,
        .mode = 0,
        .spics_io_num = PUMPS_SS_PIN,
        .queue_size = 1,
    };

    // Add device to SPI bus
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_device);
    if (ret == ESP_OK) {
        spi_initialized = true;
    }
    return ret;
}

esp_err_t spi_write(uint16_t data) {
    if (spi_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
        .tx_data = {(uint8_t)data>>8, (uint8_t)data&0xFF},
        .rx_buffer = NULL,
    };

    return spi_device_polling_transmit(spi_device, &trans);
}
