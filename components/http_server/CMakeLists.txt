idf_component_register(SRCS "http_server.c"
        INCLUDE_DIRS "include"
        REQUIRES mdns esp_http_server littlefs_helper tasks app_update esp_timer
        EMBED_FILES "frontend/wifi.html.gz" "frontend/recipe.html.gz")
