#include "littlefs_helper.h"

#include <esp_littlefs.h>
#include <esp_log.h>
#include <stdlib.h>
#include <sys/_default_fcntl.h>

#include "../../include/values.h"

static const char* TAG = "littlefs_helper";


void littlefs_mount()
{
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = LITTLEFS_BASE_PATH,
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    const esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
    }

    ESP_LOGI(TAG, "LittleFS mounted successfully");
}


FILE* open_file_to_read(const char* file_name)
{
    char path[LITTLEFS_MAX_PATH_LENGTH];
    if (snprintf(path, sizeof(path), "%s/%s", LITTLEFS_BASE_PATH, file_name) >= sizeof(path))
    {
        ESP_LOGE(TAG, "File path too long");
        return NULL;
    }

    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return NULL;
    }
    ESP_LOGI(TAG, "file opened for reading");
    return f;
}

void close_file(FILE* file)
{
    if (file != NULL)
    {
        fclose(file);
    }
}

FILE* read_from_file(const char* file_name)
{
    char path[LITTLEFS_MAX_PATH_LENGTH];
    if (snprintf(path, sizeof(path), "%s/%s", LITTLEFS_BASE_PATH, file_name) >= sizeof(path))
    {
        ESP_LOGE(TAG, "File path too long");
        return NULL;
    }

    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen(path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return NULL;
    }
    return f;
}
