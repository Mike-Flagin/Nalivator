#include <http_server.h>
#include <littlefs_helper.h>
#include <pumps.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "sdkconfig.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi_manager.h"
#include "esp_bus.h"
#include "uris.h"
#include "values.h"
#include "wifi_settings.h"

static const char TAG[] = "main";

static void init_config()
{
    const FILE* f = open_file_to_read(CONFIG_PATH);
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open config file for reading");
        return;
    }


}

void app_main(void)
{
    // Disable pumps
    init_pumps();
    disable_all_pumps();

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

        // SoftAP configuration (for captive portal)
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
    init_config();
}
