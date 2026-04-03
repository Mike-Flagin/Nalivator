idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash esp_driver_gpio spi_helper pumps touch_helper http_server littlefs_helper recipe
                       INCLUDE_DIRS "../include")
littlefs_create_partition_image(storage ../data FLASH_IN_PROJECT)