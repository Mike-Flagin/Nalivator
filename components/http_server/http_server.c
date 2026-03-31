#include "http_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <littlefs_helper.h>

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
    ssize_t chunk_size;
    do
    {
        chunk_size = fread(chunk, 1, HTTP_RESPONSE_BUFF_SIZE, file);
        if (chunk_size > 0)
        {
            if (httpd_resp_send_chunk(req, chunk, chunk_size) != ESP_OK)
            {
                ESP_LOGE(TAG, "File sending failed!");
                free(chunk);
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
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
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)wifi_html_start,
                    wifi_html_end - wifi_html_start);

}

static esp_err_t send_select_recipe_page(httpd_req_t* req)
{
    set_content_type_from_file(req, SELECT_RECIPE_PAGE_HTML);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)recipe_html_start,
                    recipe_html_end - recipe_html_start);
}

static esp_err_t redirect_to_select_recipe_page(httpd_req_t* req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", SELECT_RECIPE_PAGE_URI);
    return httpd_resp_send(req, NULL, 0);
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
}
