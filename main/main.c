#include <http_server.h>
#include <littlefs_helper.h>
#include <pumps.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "sdkconfig.h"
#include <string.h>

#include "buttons.h"
#include "nvs_flash.h"
#include "esp_wifi_manager.h"
#include "esp_bus.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "tasks.h"
#include "servo.h"
#include "oled.h"
#include "touch_helper.h"
#include "../include/uris.h"
#include "values.h"
#include "wifi_settings.h"

static const char TAG[] = "main";

static void on_client_connected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Client connected");
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        kill_display_task();
        char* buf = malloc(sizeof(char) * 128);
        sprintf(buf, "http://%s.local%s", MDNS_ADDRESS, WIFI_PAGE_URI);
        xTaskCreatePinnedToCore(oled_print_qr, "oled_task_qr", 2048, buf, 1,
                                &display_task_handle, 0);
    }
}

static void on_wifi_connected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Connected");
    if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        kill_display_task();
        static StaticTask_t display_task_buffer;
        static StackType_t display_stack[DISPLAY_TASK_STACK_SIZE];
        display_task_handle = xTaskCreateStaticPinnedToCore(oled_menu_task, "display_task", DISPLAY_TASK_STACK_SIZE, NULL,
                                                            3, display_stack, &display_task_buffer, 0);
    }
}

void app_main(void)
{
    // Disable pumps
    init_pumps();
    disable_all_pumps();

    // Initialize oled
    init_oled();

    // Initialize servo
    init_servo();

    // Initialize buttons
    init_buttons();

    // Initialize touches
    init_touches();

    // Initialize leds
    init_leds();

    // Initialize tasks
    init_tasks();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize esp_bus first (required by wifi_manager)
    ret = esp_bus_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize esp_bus: %s", esp_err_to_name(ret));
        return;
    }

    const wifi_mgr_ap_config_t ap_config = {
        .ssid = WIFI_AP_SSID,
        .password = WIFI_AP_PASS, // Open network for easy setup
        .channel = 0, // Auto channel selection
        .max_connections = 4,
        .ip = WIFI_AP_IP,
        .netmask = "255.255.255.0",
        .gateway = WIFI_AP_IP,
        .dhcp_start = "192.168.4.2",
        .dhcp_end = "192.168.4.20",
    };

    // Initialize Wi-Fi Manager
    const wifi_manager_config_t wifi_config = {
        // Retry configuration
        .max_retry_per_network = WIFI_MAXIMUM_RETRY,
        .retry_interval_ms = 5000,
        .auto_reconnect = true,

        // SoftAP configuration
        .default_ap = ap_config,
        .enable_captive_portal = false,
        .stop_ap_on_connect = true, // Stop AP after successful connection

        .mdns = {
            .enable = true,
            .hostname = MDNS_ADDRESS,
            .instance_name = MDNS_INSTANCE_NAME
        },
        .ble = {
            .enable = false
        },
        .http = {
            .enable = true,
            .httpd = NULL,
            .api_base_path = WIFI_API_URL,
            .enable_auth = false,
        },
    };

    oled_print_status("Поиск Wi-Fi", true);
    bool skip_menu_rendering = false;

    ret = wifi_manager_init(&wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi Manager: %s", esp_err_to_name(ret));
        return;
    }
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &on_client_connected, NULL, NULL));

    // Initialize http server
    init_server(wifi_manager_get_httpd());

    if (wifi_manager_wait_connected(10000) == ESP_OK)
    {
        ESP_LOGI(TAG, "WiFi connected!");
        wifi_status_t status;
        wifi_manager_get_status(&status);
        char buf[64];
        snprintf(buf, sizeof(buf), "Подключено к\n%s", status.ssid);
        char* heap_buf = strdup(buf);
        oled_print_status(heap_buf, false);
    }
    else
    {
        ESP_LOGI(TAG, "WiFi not connected, starting AP!");
        wifi_manager_start_ap(&ap_config);
        kill_display_task();
        char* buf = malloc(sizeof(char) * 128);
        sprintf(buf, "%s%s%s%s%s", "WIFI:T:WPA;S:", ap_config.ssid, ";P:", ap_config.password, ";;");
        skip_menu_rendering = true;
        xTaskCreatePinnedToCore(oled_print_qr, "oled_task_qr", 2048, buf, 1,
                                &display_task_handle, 0);
        ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connected, NULL, NULL));
    }

    // Initialize configuration
    littlefs_mount();
    xTaskCreatePinnedToCore(load_config_task, "load_config", 4096, NULL, 5, NULL, 1);

    // testing only
    FILE* fidx = open_file_to_read(RECIPES_INDEX_PATH);
    if (fidx == NULL)
    {
        xTaskCreatePinnedToCore(rebuild_index_task, "rebuild_index", 4096, NULL, 5, NULL, 1);
    }
    else
    {
        close_file(fidx);
    }
    xTaskCreate(print_system_stats, "print_system_stats", 2048, NULL, 1, NULL);

    static StaticTask_t servo_task_buffer;
    static StackType_t servo_stack[SERVO_TASK_STACK_SIZE];
    servo_task_handle = xTaskCreateStaticPinnedToCore(servo_task, "servo_task", SERVO_TASK_STACK_SIZE, NULL, 18,
                                                      servo_stack, &servo_task_buffer, 1);
    static StaticTask_t leds_task_buffer;
    static StackType_t leds_stack[LEDS_TASK_STACK_SIZE];
    xTaskCreateStaticPinnedToCore(leds_task, "leds_task", LEDS_TASK_STACK_SIZE, NULL, 18, leds_stack, &leds_task_buffer,
                                  1);
    static StaticTask_t pour_task_buffer;
    static StackType_t pour_stack[POUR_TASK_STACK_SIZE];
    pour_task_handle = xTaskCreateStaticPinnedToCore(pour_task, "pour_task", POUR_TASK_STACK_SIZE, NULL, 19, pour_stack,
                                                     &pour_task_buffer, 1);

    static StaticTask_t touches_task_buffer;
    static StackType_t touches_stack[TOUCHES_TASK_STACK_SIZE];
    touches_task_handle = xTaskCreateStaticPinnedToCore(update_touches_task, "touches_task", TOUCHES_TASK_STACK_SIZE,
                                                        NULL, 2, touches_stack, &touches_task_buffer, 0);

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_MESSAGES_DELAY_MS));
    if (!skip_menu_rendering)
    {
        kill_display_task();
        static StaticTask_t display_task_buffer;
        static StackType_t display_stack[DISPLAY_TASK_STACK_SIZE];
        display_task_handle = xTaskCreateStaticPinnedToCore(oled_menu_task, "display_task", DISPLAY_TASK_STACK_SIZE, NULL,
                                                            3, display_stack, &display_task_buffer, 0);
    }
    esp_ota_mark_app_valid_cancel_rollback();
}
