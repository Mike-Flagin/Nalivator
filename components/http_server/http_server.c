#include "http_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <littlefs_helper.h>
#include <math.h>
#include <sys/param.h>
#include <errno.h>

#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "oled.h"
#include "tasks.h"
#include "../../include/uris.h"
#include "../../include/files.h"
#include "../../include/values.h"

static const char* TAG = "http_server";

static bool has_extension(const char* filename, const char* ext)
{
    size_t filename_len = strlen(filename);
    size_t ext_len = strlen(ext);
    if (filename_len < ext_len) return false;
    return strcasecmp(filename + filename_len - ext_len, ext) == 0;
}

static esp_err_t set_content_type_from_file(httpd_req_t* req, const char* filepath)
{
    const char* type = "text/plain";

    char clean_path[256];
    snprintf(clean_path, sizeof(clean_path), "%s", filepath);

    if (has_extension(clean_path, ".gz"))
    {
        clean_path[strlen(clean_path) - 3] = '\0';
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    if (has_extension(clean_path, ".html") || has_extension(clean_path, ".htm"))
    {
        type = "text/html";
    }
    else if (has_extension(clean_path, ".js") || has_extension(clean_path, ".mjs"))
    {
        type = "application/javascript";
    }
    else if (has_extension(clean_path, ".css"))
    {
        type = "text/css";
    }
    else if (has_extension(clean_path, ".json"))
    {
        type = "application/json";
    }
    else if (has_extension(clean_path, ".xml"))
    {
        type = "text/xml";
    }
    else if (has_extension(clean_path, ".svg"))
    {
        type = "image/svg+xml";
    }
    else if (has_extension(clean_path, ".yaml") || has_extension(clean_path, ".yml"))
    {
        type = "text/yaml";
    }
    else if (has_extension(clean_path, ".toml"))
    {
        type = "application/toml";
    }

    return httpd_resp_set_type(req, type);
}

static esp_err_t send_file(httpd_req_t* req, FILE* file)
{
    char* chunk = malloc(HTTP_RESPONSE_BUFF_SIZE);
    if (chunk == NULL)
    {
        ESP_LOGE(TAG, "No memory for buffer");
        return ESP_ERR_NO_MEM;
    }
    size_t chunk_size;
    do
    {
        chunk_size = fread(chunk, 1, HTTP_RESPONSE_BUFF_SIZE, file);
        if (chunk_size > 0)
        {
            if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK) // NOLINT(*-narrowing-conversions)
            {
                ESP_LOGE(TAG, "File sending failed!");
                free(chunk);
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
        }
    }
    while (chunk_size != 0);
    free(chunk);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t static_file_handler(httpd_req_t* req)
{
    char filepath[256];
    const char* uri = req->uri;

    // Clean query-params
    const char* query_pos = strchr(uri, '?');
    size_t uri_len = query_pos ? (size_t)(query_pos - uri) : strlen(uri);

    // Redirect "/" to "/index.html"
    if (uri_len == 1 && uri[0] == '/')
    {
        snprintf(filepath, sizeof(filepath), "/index.html");
    }
    else
    {
        if (uri_len >= sizeof(filepath))
        {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        strncpy(filepath, uri, uri_len);
        filepath[uri_len] = '\0';
    }

    char gz_filepath[260];
    snprintf(gz_filepath, sizeof(gz_filepath), "%s.gz", filepath);

    // 1. Search compressed version (.gz)
    FILE* f = open_file_to_read(gz_filepath);
    if (f)
    {
        set_content_type_from_file(req, gz_filepath);
        esp_err_t ret = send_file(req, f);
        close_file(f);
        return ret;
    }

    // 2. If there is no .gz search plain file
    f = open_file_to_read(filepath);
    if (f)
    {
        set_content_type_from_file(req, filepath);
        esp_err_t ret = send_file(req, f);
        close_file(f);
        return ret;
    }

    ESP_LOGW(TAG, "File not found: %s", filepath);
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t redirect_to_select_recipe_page(httpd_req_t* req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", SELECT_RECIPE_PAGE_URI);

    //start oled
    kill_display_task();
    static StaticTask_t display_task_buffer;
    static StackType_t display_stack[DISPLAY_TASK_STACK_SIZE];
    display_task_handle = xTaskCreateStaticPinnedToCore(oled_menu_task, "display_task", DISPLAY_TASK_STACK_SIZE, NULL,
                                                        3, display_stack, &display_task_buffer, 0);
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t send_config_request(httpd_req_t* req)
{
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        FILE* f = open_file_to_read(CONFIG_PATH);
        esp_err_t ret;
        if (f)
        {
            set_content_type_from_file(req, CONFIG_PATH);
            ret = send_file(req, f);
            close_file(f);
        }
        else
        {
            set_content_type_from_file(req, DEFAULT_CONFIG_PATH);
            f = open_file_to_read(DEFAULT_CONFIG_PATH);
            ret = send_file(req, f);
            close_file(f);
        }
        xSemaphoreGive(config_mutex);
        if (ret != ESP_OK)
        {
            ret = httpd_resp_send_500(req);
        }
        return ret;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t update_config_request(httpd_req_t* req)
{
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE); // Small stack buffer for chunking
        const size_t total_len = req->content_len;
        int cur_len = 0;
        int received = 0;

        ESP_LOGI(TAG, "Starting config update, size: %d", total_len);

        FILE* f = open_file_to_write(TEMP_CONFIG_PATH);

        while (cur_len < total_len)
        {
            received = httpd_req_recv(req, buffer, sizeof(buffer));

            if (received <= 0)
            {
                if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;

                // Unexpected failure
                close_file(f);
                free(buffer);
                delete_file(TEMP_CONFIG_PATH); // Delete partial file
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }

            fwrite(buffer, 1, received, f);
            cur_len += received;
        }
        close_file(f);
        free(buffer);

        // Swap (Delete old, Rename new)
        delete_file(CONFIG_PATH); // Remove old config
        if (!rename_file(TEMP_CONFIG_PATH, CONFIG_PATH))
        {
            ESP_LOGE(TAG, "Failed to rename temp config");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Config updated successfully.");

        httpd_resp_set_status(req, "201 Created");
        httpd_resp_send(req, NULL, 0);
        xSemaphoreGive(config_mutex);
        xTaskCreatePinnedToCore(load_config_task, "load_config", 4096, NULL, 5, NULL, 1);

        return ESP_OK;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t set_pump_request(httpd_req_t* req)
{
    //deselect if any selected
    if (config.current_recipe.id != 0) free_recipe(&config.current_recipe);
    config.current_recipe.id = 0;

    //parse json
    const size_t total_len = req->content_len;
    size_t cur_len = 0;
    if (total_len >= HTTP_RESPONSE_BUFF_SIZE)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE);
    int received = 0;

    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buffer + cur_len, total_len);

        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;

            free(buffer);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        cur_len += received;
    }
    buffer[total_len] = '\0';

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    cJSON *id_obj = cJSON_GetObjectItem(root, PUMP_ID_JSON_KEY);
    cJSON *state_obj = cJSON_GetObjectItem(root, PUMP_STATE_JSON_KEY);

    uint8_t id;
    if (id_obj != NULL && cJSON_IsNumber(id_obj))
    {
        id = id_obj->valueint;
        ESP_LOGI(TAG, "Pump ID: %d", id);
    }
    else
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    if (id > PUMPS_AMOUNT - 1)
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    int8_t state;
    if (state_obj != NULL && cJSON_IsNumber(state_obj))
    {
        state = state_obj->valueint; // NOLINT(*-narrowing-conversions)
        ESP_LOGI(TAG, "Pump state: %d", state);
    }
    else
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    if (state < -1 || state > 1)
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    cJSON_Delete(root);

    disable_all_pumps();
    enable_pump(&pumps[id], state);
    httpd_resp_set_status(req, "200 OK");
    return httpd_resp_sendstr(req, "");
}

static esp_err_t update_firmware_request(httpd_req_t* req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Begin the OTA process
    if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle) != ESP_OK)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char* buf = malloc(HTTP_RESPONSE_BUFF_SIZE);
    if (!buf)
    {
        esp_ota_end(ota_handle);
        return ESP_ERR_NO_MEM;
    }

    size_t received = 0;
    size_t remaining = req->content_len;

    // Read the binary file from the browser in small chunks
    while (remaining > 0)
    {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, HTTP_RESPONSE_BUFF_SIZE))) <= 0)
        {
            free(buf);
            esp_ota_end(ota_handle);
            return ESP_FAIL;
        }

        // Write the chunk directly to flash memory
        if (esp_ota_write(ota_handle, buf, received) != ESP_OK)
        {
            free(buf);
            esp_ota_end(ota_handle);
            return ESP_FAIL;
        }
        remaining -= received;
    }

    free(buf);

    // Validate and set the new boot partition
    if (esp_ota_end(ota_handle) == ESP_OK)
    {
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
        {
            httpd_resp_set_status(req, "200 OK");
            httpd_resp_sendstr(req, "Firmware updated successfully.");

            // Allow time for the HTTP response to send, then restart
            const esp_timer_create_args_t restart_timer_args = {
                .callback = &restart_esp,
                .name = "restart_timer"
            };

            esp_timer_handle_t restart_timer;
            esp_timer_create(&restart_timer_args, &restart_timer);
            esp_timer_start_once(restart_timer, 2000000);
            return ESP_OK;
        }
    }

    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t restart_request_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "Restarting.");

    // Allow time for the HTTP response to send, then restart
    const esp_timer_create_args_t restart_timer_args = {
        .callback = &restart_esp,
        .name = "restart_timer"
    };

    esp_timer_handle_t restart_timer;
    esp_timer_create(&restart_timer_args, &restart_timer);
    esp_timer_start_once(restart_timer, 2000000);
    return ESP_OK;
}

static esp_err_t send_ingredients_request(httpd_req_t* req)
{
    if (xSemaphoreTake(ingredients_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        FILE* f = open_file_to_read(INGREDIENTS_PATH);
        esp_err_t ret;
        if (f)
        {
            set_content_type_from_file(req, INGREDIENTS_PATH);
            ret = send_file(req, f);
            close_file(f);
            xSemaphoreGive(ingredients_mutex);
        }
        else
        {
            ret = ESP_FAIL;
        }
        if (ret != ESP_OK)
        {
            ret = httpd_resp_send_500(req);
        }
        return ret;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t update_ingredients_request(httpd_req_t* req)
{
    if (xSemaphoreTake(ingredients_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE); // Small stack buffer for chunking
        const size_t total_len = req->content_len;
        int cur_len = 0;
        int received = 0;

        ESP_LOGI(TAG, "Starting ingredients update, size: %d", total_len);

        FILE* f = open_file_to_write(TEMP_INGREDIENTS_PATH);

        while (cur_len < total_len)
        {
            received = httpd_req_recv(req, buffer, HTTP_RESPONSE_BUFF_SIZE);

            if (received <= 0)
            {
                if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;

                // Unexpected failure
                close_file(f);
                free(buffer);
                delete_file(TEMP_INGREDIENTS_PATH); // Delete partial file
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }

            fwrite(buffer, 1, received, f);
            cur_len += received;
        }
        close_file(f);
        free(buffer);

        // Swap (Delete old, Rename new)
        // Remove old config
        delete_file(INGREDIENTS_PATH);
        if (!rename_file(TEMP_INGREDIENTS_PATH, INGREDIENTS_PATH))
        {
            ESP_LOGE(TAG, "Rename failed! Error: %s", strerror(errno));
            xSemaphoreGive(ingredients_mutex);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Ingredients updated successfully.");

        httpd_resp_set_status(req, "201 Created");
        httpd_resp_send(req, NULL, 0);
        xSemaphoreGive(ingredients_mutex);

        return ESP_OK;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t send_recipes_request(httpd_req_t* req)
{
    if (xSemaphoreTake(recipes_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        FILE* f = open_file_to_read(RECIPES_PATH);
        esp_err_t ret;
        if (f)
        {
            set_content_type_from_file(req, RECIPES_PATH);
            ret = send_file(req, f);
            close_file(f);
            xSemaphoreGive(recipes_mutex);
        }
        else
        {
            ret = ESP_FAIL;
        }
        if (ret != ESP_OK)
        {
            ret = httpd_resp_send_500(req);
        }
        return ret;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t update_recipes_request(httpd_req_t* req)
{
    if (xSemaphoreTake(recipes_mutex, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
        char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE); // Small stack buffer for chunking
        const size_t total_len = req->content_len;
        int cur_len = 0;
        int received = 0;

        ESP_LOGI(TAG, "Starting recipes update, size: %d", total_len);

        FILE* f = open_file_to_write(TEMP_RECIPES_PATH);

        while (cur_len < total_len)
        {
            received = httpd_req_recv(req, buffer, HTTP_RESPONSE_BUFF_SIZE);

            if (received <= 0)
            {
                if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;

                // Unexpected failure
                close_file(f);
                free(buffer);
                delete_file(TEMP_RECIPES_PATH); // Delete partial file
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }

            fwrite(buffer, 1, received, f);
            cur_len += received;
        }
        close_file(f);
        free(buffer);

        // Swap (Delete old, Rename new)
        // Remove old config
        delete_file(RECIPES_PATH);
        if (!rename_file(TEMP_RECIPES_PATH, RECIPES_PATH))
        {
            ESP_LOGE(TAG, "Rename failed! Error: %s", strerror(errno));
            httpd_resp_send_500(req);
            xSemaphoreGive(recipes_mutex);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Recipes updated successfully.");

        httpd_resp_set_status(req, "201 Created");
        httpd_resp_send(req, NULL, 0);
        xSemaphoreGive(recipes_mutex);

        xTaskCreatePinnedToCore(rebuild_index_task, "rebuild_index", 4096, NULL, 5, NULL, 1);
        //deselect if any selected
        if (config.current_recipe.id != 0) free_recipe(&config.current_recipe);
        config.current_recipe.id = 0;

        return ESP_OK;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t select_recipe_request(httpd_req_t* req)
{
    const size_t total_len = req->content_len;
    size_t cur_len = 0;
    if (total_len >= HTTP_RESPONSE_BUFF_SIZE)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE);
    int received = 0;

    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buffer + cur_len, total_len);

        if (received <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;

            free(buffer);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        cur_len += received;
    }
    buffer[total_len] = '\0';

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    cJSON *id_item = cJSON_GetObjectItem(root, RECIPE_ID_JSON_KEY);
    cJSON *portion_item = cJSON_GetObjectItem(root, RECIPE_PORTION_JSON_KEY);

    uint16_t id;
    if (id_item != NULL && cJSON_IsNumber(id_item))
    {
        id = id_item->valueint;
        ESP_LOGI(TAG, "Recipe ID: %d", id);
    }
    else
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    // if deselect
    if (id == 0)
    {
        cJSON_Delete(root);
        const recipe_t res = {0};
        if (config.current_recipe.id != 0) free_recipe(&config.current_recipe);
        config.current_recipe = res;
        httpd_resp_set_status(req, "200 OK");
        return httpd_resp_send(req, NULL, 0);
    }

    float portion;
    if (portion_item != NULL && cJSON_IsNumber(portion_item))
    {
        portion = (float)portion_item->valuedouble;
        ESP_LOGI(TAG, "Recipe portion: %.0f", portion);
    }
    else
    {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    cJSON_Delete(root);

    //if portion value is invalid
    if (portion <= 0)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, NULL, 0);
    }

    // if recipe not found
    if (!check_recipe_id(id))
    {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_send(req, NULL, 0);
    }

    recipe_t recipe = get_recipe(id);
    //if reading failed
    if (recipe.id == 0)
    {
        return httpd_resp_send_500(req);
    }

    // if ingredients missing
    if (!check_recipe(&recipe))
    {
        httpd_resp_set_status(req, "406 Not Acceptable");
        free_recipe(&recipe);
        return httpd_resp_send(req, NULL, 0);
    }

    //clean previous selection
    if (config.current_recipe.id != 0) free_recipe(&config.current_recipe);

    config.current_recipe = recipe;
    config.portion = portion;
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t get_selected_recipe_request(httpd_req_t* req)
{
    set_content_type_from_file(req, ".json");

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, RECIPE_ID_JSON_KEY, config.current_recipe.id);
    cJSON_AddNumberToObject(root, RECIPE_PORTION_JSON_KEY, config.portion);

    char* json = cJSON_Print(root);

    cJSON_Delete(root);

    httpd_resp_sendstr(req, json);
    free(json);
    return ESP_OK;
}

void init_server(httpd_handle_t server)
{
    // APIs
    static const httpd_uri_t wifi_dont_connect_uri = {
        .uri = WIFI_DONT_CONNECT_URI,
        .method = HTTP_POST,
        .handler = redirect_to_select_recipe_page,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_dont_connect_uri);

    // Configuration
    static const httpd_uri_t get_config_request = {
        .uri = CONFIG_URI,
        .method = HTTP_GET,
        .handler = send_config_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_config_request);

    static const httpd_uri_t put_config_request = {
        .uri = CONFIG_URI,
        .method = HTTP_PUT,
        .handler = update_config_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &put_config_request);

    static const httpd_uri_t post_pump_request = {
        .uri = SET_PUMP_STATE_URI,
        .method = HTTP_POST,
        .handler = set_pump_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &post_pump_request);

    static const httpd_uri_t ota_update_request = {
        .uri = UPDATE_URI,
        .method = HTTP_POST,
        .handler = update_firmware_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_update_request);

    static const httpd_uri_t restart_request = {
        .uri = RESTART_URI,
        .method = HTTP_POST,
        .handler = restart_request_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &restart_request);

    // Ingredients
    static const httpd_uri_t get_ingredients_request = {
        .uri = INGREDIENTS_URI,
        .method = HTTP_GET,
        .handler = send_ingredients_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_ingredients_request);

    static const httpd_uri_t put_ingredients_request = {
        .uri = INGREDIENTS_URI,
        .method = HTTP_PUT,
        .handler = update_ingredients_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &put_ingredients_request);

    // Recipes
    static const httpd_uri_t get_recipes_request = {
        .uri = RECIPES_URI,
        .method = HTTP_GET,
        .handler = send_recipes_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_recipes_request);

    static const httpd_uri_t put_recipes_request = {
        .uri = RECIPES_URI,
        .method = HTTP_PUT,
        .handler = update_recipes_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &put_recipes_request);

    static const httpd_uri_t select_recipe_request_handler = {
        .uri = SELECT_RECIPE_URI,
        .method = HTTP_POST,
        .handler = select_recipe_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &select_recipe_request_handler);

    static const httpd_uri_t get_selected_recipe_request_handler = {
        .uri = SELECT_RECIPE_URI,
        .method = HTTP_GET,
        .handler = get_selected_recipe_request,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_selected_recipe_request_handler);

    static const httpd_uri_t static_file_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &static_file_uri);
}
