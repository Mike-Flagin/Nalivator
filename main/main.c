#include <http_server.h>
#include <littlefs_helper.h>
#include <pumps.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "cJSON.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "sdkconfig.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi_manager.h"
#include "esp_bus.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "tasks.h"
#include "../include/files.h"
#include "../include/uris.h"
#include "values.h"
#include "wifi_settings.h"

static const char TAG[] = "main";

static void init_config(const char* path)
{
    ESP_LOGI(TAG, "Attempting to load config: %s", path);

    FILE* f = open_file_to_read(path);
    char* json_buffer = NULL;
    cJSON* root = NULL;
    bool success = false;

    if (f != NULL)
    {
        fseek(f, 0, SEEK_END);
        const long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (file_size > 0)
        {
            json_buffer = malloc(file_size + 1);
            if (json_buffer != NULL)
            {
                const size_t bytes_read = fread(json_buffer, 1, file_size, f);
                json_buffer[bytes_read] = '\0';

                root = cJSON_Parse(json_buffer);
                if (root != NULL)
                {
                    const cJSON* pumps_json = cJSON_GetObjectItem(root, PUMPS_JSON_KEY);
                    if (cJSON_IsArray(pumps_json))
                    {
                        // Stack allocation for the configuration arrays
                        uint16_t flowrates[PUMPS_AMOUNT] = {0};
                        bool inverses[PUMPS_AMOUNT] = {false};
                        uint16_t ingredients[PUMPS_AMOUNT] = {0};
                        uint8_t volumes_to_splitter[PUMPS_AMOUNT] = {0};

                        for (int i = 0; i < PUMPS_AMOUNT; i++)
                        {
                            cJSON* p = cJSON_GetArrayItem(pumps_json, i);
                            if (!p) continue;

                            flowrates[i] = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(p, FLOWRATE_JSON_KEY));
                            inverses[i] = cJSON_IsTrue(cJSON_GetObjectItem(p, INVERSE_JSON_KEY));
                            ingredients[i] = (uint16_t)
                                cJSON_GetNumberValue(cJSON_GetObjectItem(p, INGREDIENT_JSON_KEY));
                            volumes_to_splitter[i] = (uint8_t)cJSON_GetNumberValue(
                                cJSON_GetObjectItem(p, VOLUME_TO_SPLITTER_JSON_KEY));
                        }

                        configure_pumps(flowrates, inverses, ingredients, volumes_to_splitter);


                        // Parse servoPositions array
                        // TODO: implement servo logic
                        const cJSON* servo_positions_json = cJSON_GetObjectItem(root, SERVO_POSITIONS_JSON_KEY);


                        // Parse volumeAfterSplitter
                        // TODO: implement volume after splitter logic
                        const cJSON* volume_after_splitter_json = cJSON_GetObjectItem(
                            root, VOLUME_AFTER_SPLITTER_JSON_KEY);


                        // Parse LED colors arrays
                        // TODO: implement color logic
                        success = true;
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Pumps array missing in %s", path);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "JSON parse error in %s", path);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Memory allocation failed for %s", path);
            }
        }
        close_file(f);
    }

    // Cleanup current attempt
    if (root) cJSON_Delete(root);
    if (json_buffer) free(json_buffer);

    // Final result check
    if (success)
    {
        ESP_LOGI(TAG, "Configuration applied from %s", path);
    }
    else
    {
        // Prevent infinite recursion if default also fails
        if (strcmp(path, DEFAULT_CONFIG_PATH) != 0)
        {
            ESP_LOGW(TAG, "Main config failed. Falling back to default...");
            init_config(DEFAULT_CONFIG_PATH);
        }
        else
        {
            ESP_LOGE(TAG, "Default configuration failed to load!");
        }
    }
}

void app_main(void)
{
    // Disable pumps
    init_pumps();
    disable_all_pumps();

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
        .ip = "192.168.4.1",
        .netmask = "255.255.255.0",
        .gateway = "192.168.4.1",
        .dhcp_start = "192.168.4.2",
        .dhcp_end = "192.168.4.20",
    };

    // Initialize Wi-Fi Manager
    const wifi_manager_config_t config = {
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

    ret = wifi_manager_init(&config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi Manager: %s", esp_err_to_name(ret));
        return;
    }
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Initialize http server
    init_server(wifi_manager_get_httpd());

    if (wifi_manager_wait_connected(10000) == ESP_OK)
    {
        ESP_LOGI(TAG, "WiFi connected!");
    }
    else
    {
        ESP_LOGI(TAG, "WiFi not connected, starting AP!");
        wifi_manager_start_ap(&ap_config);
    }

    // Initialize configuration
    littlefs_mount();
    init_config(CONFIG_PATH);

    esp_ota_mark_app_valid_cancel_rollback();
}
