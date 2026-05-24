idf_component_register(SRCS "oled.c"
        INCLUDE_DIRS "include"
        REQUIRES esp_driver_i2c u8g2 freertos qr_code_generator touch_helper recipe tasks)
