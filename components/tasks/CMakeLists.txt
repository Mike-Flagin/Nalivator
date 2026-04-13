idf_component_register(SRCS "tasks.c"
        INCLUDE_DIRS "include"
        REQUIRES freertos recipe pumps leds buttons servo)
