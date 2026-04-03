#include "http_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <littlefs_helper.h>
#include <sys/unistd.h>

#include "mdns.h"
#include "uris.h"
#include "files.h"
#include "../../include/values.h"

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

static const char* TAG = "http_server";

extern const uint8_t wifi_html_start[] asm("_binary_wifi_html_gz_start");
extern const uint8_t wifi_html_end[] asm("_binary_wifi_html_gz_end");
extern const uint8_t recipe_html_start[] asm("_binary_recipe_html_gz_start");
extern const uint8_t recipe_html_end[] asm("_binary_recipe_html_gz_end");

static esp_err_t set_content_type_from_file(httpd_req_t* req, const char* filepath)
{
    const char* type = "text/plain";
    if (strstr(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (strstr(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (strstr(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (strstr(filepath, ".json"))
    {
        type = "application/json";
    }

    if (CHECK_FILE_EXTENSION(filepath, ".gz"))
    {
        const esp_err_t r = httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        if (r != ESP_OK) return r;
    }
    return httpd_resp_set_type(req, type);
}

static esp_err_t send_file(httpd_req_t* req, FILE* file)
{
    char *chunk = malloc(HTTP_RESPONSE_BUFF_SIZE);
    if (chunk == NULL) {
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
    return ESP_OK;
}

static esp_err_t send_wifi_page(httpd_req_t* req)
{
    set_content_type_from_file(req, WIFI_PAGE_HTML);
    return httpd_resp_send(req, (const char *)wifi_html_start,
                    wifi_html_end - wifi_html_start);

}

static esp_err_t send_select_recipe_page(httpd_req_t* req)
{
    set_content_type_from_file(req, SELECT_RECIPE_PAGE_HTML);
    return httpd_resp_send(req, (const char *)recipe_html_start,
                    recipe_html_end - recipe_html_start);
}

static esp_err_t redirect_to_select_recipe_page(httpd_req_t* req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", SELECT_RECIPE_PAGE_URI);
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t send_config_request(httpd_req_t* req)
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
    if (ret != ESP_OK)
    {
        ret = httpd_resp_send_500(req);
    }
    return ret;
}

static esp_err_t update_config_request(httpd_req_t* req)
{
    char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE); // Small stack buffer for chunking
    const size_t total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    ESP_LOGI(TAG, "Starting config update, size: %d", total_len);

    FILE* f = open_file_to_write(TEMP_CONFIG_PATH);

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buffer, sizeof(buffer));

        if (received <= 0) {
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
    if (!rename_file(TEMP_CONFIG_PATH, CONFIG_PATH)) {
        ESP_LOGE(TAG, "Failed to rename temp config");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Config updated successfully.");

    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);

    //TODO: Create task on core 1 to load new config

    return ESP_OK;
}

static esp_err_t send_ingredients_request(httpd_req_t* req)
{
    FILE* f = open_file_to_read(INGREDIENTS_PATH);
    esp_err_t ret;
    if (f)
    {
        set_content_type_from_file(req, INGREDIENTS_PATH);
        ret = send_file(req, f);
        close_file(f);
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

static esp_err_t update_ingredients_request(httpd_req_t* req)
{
    char* buffer = malloc(sizeof(char) * HTTP_RESPONSE_BUFF_SIZE); // Small stack buffer for chunking
    const size_t total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    ESP_LOGI(TAG, "Starting ingredients update, size: %d", total_len);

    FILE* f = open_file_to_write(TEMP_INGREDIENTS_PATH);

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buffer, HTTP_RESPONSE_BUFF_SIZE);

        if (received <= 0) {
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
    if (!rename_file(TEMP_INGREDIENTS_PATH, INGREDIENTS_PATH)) {
        ESP_LOGE(TAG, "Failed to rename temp file");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ingredients updated successfully.");

    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

void init_server(httpd_handle_t server)
{
    //register URIs
    //Wi-Fi page
    static const httpd_uri_t wifi_page_uri = {
        .uri = WIFI_PAGE_URI,
        .method = HTTP_GET,
        .handler = send_wifi_page,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_page_uri);

    static const httpd_uri_t wifi_dont_connect_uri = {
        .uri = WIFI_DONT_CONNECT_URI,
        .method = HTTP_POST,
        .handler = redirect_to_select_recipe_page,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_dont_connect_uri);

    //select recipe page
    static const httpd_uri_t select_recipe_page_uri = {
        .uri = SELECT_RECIPE_PAGE_URI,
        .method = HTTP_GET,
        .handler = send_select_recipe_page,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &select_recipe_page_uri);

    // APIs
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

}
