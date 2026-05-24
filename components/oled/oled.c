//
// Created by mikes on 08.05.2026.
//

#include "include/oled.h"

#include "esp_log.h"
#include "qrcodegen.h"
#include "recipe.h"
#include "tasks.h"
#include "touch_helper.h"
#include "driver/i2c_master.h"
#include "u8g2.h"
#include "../../include/pins.h"
#include "freertos/projdefs.h"
#include "freertos/event_groups.h"

static char* TAG = "OLED";

TaskHandle_t display_task_handle = NULL;
EventGroupHandle_t display_events = NULL;
SemaphoreHandle_t display_i2c_mutex = NULL;

static i2c_master_dev_handle_t dev_handle;
u8g2_t u8g2;

uint8_t u8g2_esp32_v5_i2c_byte_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    static uint8_t buffer[64];
    static uint8_t buf_idx;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        // I2C peripheral lifecycle is handled natively in app_main
        break;

    case U8X8_MSG_BYTE_SET_DC:
        // Unused for native I2C, but required by U8g2 interface structure
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;

    case U8X8_MSG_BYTE_SEND:
        {
            uint8_t* data = arg_ptr;
            while (arg_int > 0)
            {
                buffer[buf_idx++] = *data;
                data++;
                arg_int--;
            }
            break;
        }

    case U8X8_MSG_BYTE_END_TRANSFER:
        {
            // Using the new ESP-IDF v5 transmit API explicitly
            esp_err_t err = i2c_master_transmit(dev_handle, buffer, buf_idx, -1);
            if (err != ESP_OK)
            {
                return 0; // Return 0 to U8g2 to flag transmission failure
            }
            break;
        }
    }
    return 1;
}

uint8_t u8g2_esp32_v5_gpio_and_delay_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    default:
        break;
    }
    return 1;
}

void init_oled(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = OLED_SDA_PIN,
        .scl_io_num = OLED_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_DISPLAY_ADDRESS,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_v5_i2c_byte_cb, u8g2_esp32_v5_gpio_and_delay_cb);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    display_i2c_mutex = xSemaphoreCreateMutex();
    if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
    {
        u8g2_SendBuffer(&u8g2);
        xSemaphoreGive(display_i2c_mutex);
    }
    u8g2_SetFont(&u8g2, u8g2_font_9x15_t_cyrillic);

    ESP_LOGI(TAG, "Display initialization completed");
    display_events = xEventGroupCreate();
}

void kill_display_task()
{
    if (display_task_handle != NULL)
    {
        if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
        {
            TaskHandle_t target_task = display_task_handle;

            display_task_handle = NULL;

            if (target_task != NULL)
            {
                vTaskDelete(target_task);
            }

            xSemaphoreGive(display_i2c_mutex);
        }
    }
}

void oled_print_status(const char* text, const bool animated)
{
    kill_display_task();

    if (animated)
    {
        xTaskCreatePinnedToCore(show_status_animated, "oled_task_status_animated", 3072, (void*)text, 1,
                                &display_task_handle, 0);
    }
    else
    {
        xTaskCreatePinnedToCore(show_status_static, "oled_task_status_static", 3072, (void*)text, 1,
                                &display_task_handle, 0);
    }
}

void show_status_animated(void* pvParameters)
{
    const char* base_text = (const char*)pvParameters;
    u8g2_SetFontPosTop(&u8g2);

    const u8g2_uint_t screen_width = u8g2_GetDisplayWidth(&u8g2);
    const u8g2_uint_t screen_height = u8g2_GetDisplayHeight(&u8g2);

    uint8_t dot_frame = 0;
    char display_buffer[128];
    char max_len_test[128];

    ESP_LOGI(TAG, "Started animation task for text: %s", base_text);

    while (1)
    {
        switch (dot_frame)
        {
        case 0: snprintf(display_buffer, sizeof(display_buffer), "%s", base_text);
            break;
        case 1: snprintf(display_buffer, sizeof(display_buffer), "%s.", base_text);
            break;
        case 2: snprintf(display_buffer, sizeof(display_buffer), "%s..", base_text);
            break;
        case 3: snprintf(display_buffer, sizeof(display_buffer), "%s...", base_text);
            break;
        default: snprintf(display_buffer, sizeof(display_buffer), "%s", base_text);
            break;
        }

        snprintf(max_len_test, sizeof(max_len_test), "%s...", base_text);
        const u8g2_uint_t max_width = u8g2_GetUTF8Width(&u8g2, max_len_test);

        const u8g2_uint_t text_x = (screen_width > max_width) ? (screen_width - max_width) / 2 : 0;
        const u8g2_uint_t text_y = (screen_height / 2) - (u8g2_GetMaxCharHeight(&u8g2) / 2);

        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawUTF8(&u8g2, text_x, text_y, display_buffer);
        if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
        {
            u8g2_SendBuffer(&u8g2);
            xSemaphoreGive(display_i2c_mutex);
        }
        dot_frame = (dot_frame + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(350));
    }
}

void show_status_static(void* pvParameters)
{
    const char* base_text = (const char*)pvParameters;
    u8g2_SetFontPosTop(&u8g2);

    const u8g2_uint_t screen_width = u8g2_GetDisplayWidth(&u8g2);
    const u8g2_uint_t screen_height = u8g2_GetDisplayHeight(&u8g2);

    char display_buffer[128];
    snprintf(display_buffer, sizeof(display_buffer), "%s", base_text);

    // Set up an array to hold pointers for each line
    char* lines[LINES_PER_PAGE];
    uint8_t line_count = 0;

    // The first line always starts at the beginning of the buffer
    lines[line_count++] = display_buffer;

    // Parse the buffer: replace '\n' with '\0' and store the start of the next line
    char* ptr = display_buffer;
    while (*ptr != '\0')
    {
        if (*ptr == '\n')
        {
            *ptr = '\0'; // Terminate the current line
            if (line_count < LINES_PER_PAGE)
            {
                lines[line_count++] = ptr + 1; // Save pointer to the start of the next line
            }
        }
        ptr++;
    }

    u8g2_ClearBuffer(&u8g2);

    // Calculate positioning to perfectly center the entire text block vertically
    const u8g2_uint_t char_height = u8g2_GetMaxCharHeight(&u8g2);
    const u8g2_uint_t line_spacing = 1; // 1-pixel gap
    const u8g2_uint_t step_y = char_height + line_spacing;

    // Total height of the text block = (number of lines * step size) minus the final gap
    const u8g2_uint_t total_block_height = (line_count * step_y) - line_spacing;

    // Starting Y coordinate so the block sits perfectly in the middle of the screen
    u8g2_uint_t current_y = (screen_height > total_block_height) ? (screen_height - total_block_height) / 2 : 0;

    // Render each line centered horizontally
    for (uint8_t i = 0; i < line_count; i++)
    {
        u8g2_uint_t text_width = u8g2_GetUTF8Width(&u8g2, lines[i]);
        u8g2_uint_t current_x = (screen_width > text_width) ? (screen_width - text_width) / 2 : 0;

        u8g2_DrawUTF8(&u8g2, current_x, current_y, lines[i]);

        current_y += step_y; // Move down for the next line
    }

    // Send to display and cleanup
    if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
    {
        u8g2_SendBuffer(&u8g2);
        xSemaphoreGive(display_i2c_mutex);
    }

    free(pvParameters);
    display_task_handle = NULL;
    vTaskDelete(NULL);
}

void oled_print_qr(void* pvParameters)
{
    const char* text = (const char*)pvParameters;

    u8g2_ClearBuffer(&u8g2);
    uint8_t* tempBuffer = malloc(sizeof(uint8_t) * qrcodegen_BUFFER_LEN_FOR_VERSION(3));
    uint8_t* qr = malloc(sizeof(uint8_t) * qrcodegen_BUFFER_LEN_FOR_VERSION(3));
    if (qrcodegen_encodeText(text, tempBuffer, qr, qrcodegen_Ecc_LOW,
                             3, 3,
                             qrcodegen_Mask_AUTO, true))
    {
        //scale and show
        const uint8_t qr_size = qrcodegen_getSize(qr);
        const uint8_t left_offset = u8g2_GetDisplayWidth(&u8g2) / 2 - qr_size;
        const uint8_t top_offset = u8g2_GetDisplayHeight(&u8g2) / 2 - qr_size;
        for (uint8_t y = 0; y < qr_size; y++)
        {
            for (uint8_t x = 0; x < qr_size; x++)
            {
                if (!qrcodegen_getModule(qr, x, y))
                {
                    u8g2_DrawBox(&u8g2, left_offset + x * 2, top_offset + y * 2, 2, 2);
                }
            }
        }
        if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
        {
            u8g2_SendBuffer(&u8g2);
            xSemaphoreGive(display_i2c_mutex);
        }
        free(tempBuffer);
        free(qr);
        display_task_handle = NULL;
        free(pvParameters);
        vTaskDelete(NULL);
    }

    uint8_t* tempBuffer2 = realloc(tempBuffer, sizeof(uint8_t) * qrcodegen_BUFFER_LEN_FOR_VERSION(10));
    if (tempBuffer2 == NULL)
    {
        ESP_LOGE(TAG, "Failed to reallocate memory");
        free(tempBuffer);
        display_task_handle = NULL;
        free(pvParameters);
        vTaskDelete(NULL);
    }
    tempBuffer = tempBuffer2;
    uint8_t* qr2 = realloc(qr, sizeof(uint8_t) * qrcodegen_BUFFER_LEN_FOR_VERSION(10));
    if (qr2 == NULL)
    {
        ESP_LOGE(TAG, "Failed to reallocate memory");
        free(qr);
        display_task_handle = NULL;
        free(pvParameters);
        vTaskDelete(NULL);
    }
    qr = qr2;

    if (qrcodegen_encodeText(text, tempBuffer, qr, qrcodegen_Ecc_LOW,
                             10, 10,
                             qrcodegen_Mask_AUTO, true))
    {
        //show
        const uint8_t qr_size = qrcodegen_getSize(qr);
        const uint8_t left_offset = u8g2_GetDisplayWidth(&u8g2) / 2 - qr_size / 2;
        const uint8_t top_offset = u8g2_GetDisplayHeight(&u8g2) / 2 - qr_size / 2;
        for (uint8_t y = 0; y < qr_size; y++)
        {
            for (uint8_t x = 0; x < qr_size; x++)
            {
                if (!qrcodegen_getModule(qr, x, y))
                {
                    u8g2_DrawBox(&u8g2, left_offset + x, top_offset + y, 1, 1);
                }
            }
        }
        if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
        {
            u8g2_SendBuffer(&u8g2);
            xSemaphoreGive(display_i2c_mutex);
        }
        free(tempBuffer);
        free(qr);
        display_task_handle = NULL;
        free(pvParameters);
        vTaskDelete(NULL);
    }

    ESP_LOGE(TAG, "Could not encode text");
    free(tempBuffer);
    free(qr);
    display_task_handle = NULL;
    free(pvParameters);
    vTaskDelete(NULL);
}

void oled_menu_task(void* pvParameters)
{
    uint8_t recipes_count = 0;
    char** names = NULL;
    if (xSemaphoreTake(recipes_mutex, portMAX_DELAY))
    {
        recipes_count = get_recipes_count();
        if (recipes_count > 0)
        {
            names = malloc(sizeof(char*) * recipes_count);
            for (uint16_t i = 0; i < recipes_count; i++)
            {
                names[i] = get_recipe_name(i + 1);
            }
        }

        xSemaphoreGive(recipes_mutex);
    }

    uint16_t menu_pointer = 0;
    bool selection_mode = false;
    float portion_selection = 1;
    uint16_t current_volume = 0;

    int64_t last_interaction_time = esp_timer_get_time();
    bool is_screen_off = false;

    uint16_t last_menu_pointer = 0xFFFF;
    int16_t scroll_x = 0;
    uint16_t scroll_wait = 0;

    while (1)
    {
        //update recipes if needed
        const EventBits_t bits = xEventGroupWaitBits(display_events, RECIPES_UPDATED_BIT, pdTRUE,pdFALSE, 0);
        if ((bits & RECIPES_UPDATED_BIT) != 0)
        {
            if (xSemaphoreTake(recipes_mutex, portMAX_DELAY))
            {
                for (uint16_t i = 0; i < recipes_count; i++)
                {
                    free(names[i]);
                }
                free(names);
                recipes_count = get_recipes_count();
                names = malloc(sizeof(char*) * recipes_count);
                for (uint16_t i = 0; i < recipes_count; i++)
                {
                    names[i] = get_recipe_name(i + 1);
                }
                menu_pointer = 0;
                ESP_LOGI(TAG, "Recipes count updated! Total: %lu", recipes_count);
                xSemaphoreGive(recipes_mutex);
            }
        }

        if (recipes_count == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_TIME_MS));
            continue;
        }
        u8g2_ClearBuffer(&u8g2);

        if (is_next_pressed() || is_prev_pressed() || is_next_hold() || is_prev_hold())
        {
            last_interaction_time = esp_timer_get_time(); // Reset the timer

            if (is_screen_off)
            {
                is_screen_off = false;
                if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
                {
                    u8g2_SetPowerSave(&u8g2, 0); // Turn OLED hardware ON
                    xSemaphoreGive(display_i2c_mutex);
                }

                // Block for 500ms so "wake up" touch isn't registered as a click
                vTaskDelay(pdMS_TO_TICKS(DISPLAY_AFTER_DOUBLE_CLICK_DELAY_MS));
                continue;
            }
        }

        if (!is_screen_off && (esp_timer_get_time() - last_interaction_time > DISPLAY_TIMEOUT_US))
        {
            is_screen_off = true;
            if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
            {
                u8g2_ClearBuffer(&u8g2);     // Clear internal RAM
                u8g2_SendBuffer(&u8g2);      // Push empty RAM to screen
                u8g2_SetPowerSave(&u8g2, 1); // Turn OLED hardware OFF
                xSemaphoreGive(display_i2c_mutex);
            }
        }

        if (is_screen_off)
        {
            vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_TIME_MS));
            continue;
        }

        if (selection_mode)
        {
            if (is_next_pressed() && is_prev_pressed())
            {
                ESP_LOGI(TAG, "Select Recipe");
                selection_mode = false;

                // select
                recipe_t recipe = get_recipe(menu_pointer + 1);
                // if ingredients missing
                if (!check_recipe(&recipe))
                {
                    free_recipe(&recipe);

                    u8g2_DrawUTF8(&u8g2, 50, LINE_HEIGHT, "Нет");
                    u8g2_DrawUTF8(&u8g2, 5, LINE_HEIGHT * 2, "ингредиентов!");

                    vTaskDelay(pdMS_TO_TICKS(DISPLAY_MESSAGES_DELAY_MS));
                }
                else
                {
                    //clean previous selection
                    if (config.current_recipe.id != 0) free_recipe(&config.current_recipe);
                    config.current_recipe = recipe;
                    config.portion = portion_selection;

                    u8g2_DrawUTF8(&u8g2, 5, LINE_HEIGHT * 2, "Рецепт выбран!");
                }

                if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
                {
                    u8g2_SendBuffer(&u8g2);
                    xSemaphoreGive(display_i2c_mutex);
                }

                vTaskDelay(pdMS_TO_TICKS(DISPLAY_MESSAGES_DELAY_MS));
            }
            else if (is_next_pressed() || is_next_hold())
            {
                portion_selection += 0.5f;
                if (portion_selection > 100) portion_selection = 100.0f;
            }
            else if (is_prev_pressed() || is_prev_hold())
            {
                portion_selection -= 0.5f;
                if (portion_selection <= 0.4) portion_selection = 0.5f;
            }
        }
        else
        {
            if (is_next_pressed() && is_prev_pressed())
            {
                selection_mode = true;
                portion_selection = 1;

                current_volume = get_recipe_volume(menu_pointer + 1);
                vTaskDelay(pdMS_TO_TICKS(DISPLAY_AFTER_DOUBLE_CLICK_DELAY_MS));
            }
            else if (is_next_pressed() || is_next_hold())
            {
                if (menu_pointer == recipes_count - 1) menu_pointer = 0;
                else menu_pointer++;
            }
            else if (is_prev_pressed() || is_prev_hold())
            {
                if (menu_pointer == 0) menu_pointer = recipes_count - 1;
                else menu_pointer--;
            }

            // Reset scrolling if the user moved the pointer
            if (menu_pointer != last_menu_pointer)
            {
                last_menu_pointer = menu_pointer;
                scroll_x = 0;
                scroll_wait = 0;
            }
        }
        u8g2_SetFontPosTop(&u8g2);

        if (selection_mode)
        {
            u8g2_DrawUTF8(&u8g2, 0, 0, "Выберите");
            u8g2_DrawUTF8(&u8g2, 0, LINE_HEIGHT, "объём:");
            char buf[32];
            snprintf(buf, sizeof(buf), "%d x %.1f =", current_volume, portion_selection);
            u8g2_DrawUTF8(&u8g2, 0, LINE_HEIGHT * 2, buf);
            snprintf(buf, sizeof(buf), "%.1f мл", (float)current_volume * portion_selection);
            u8g2_DrawUTF8(&u8g2, 0, LINE_HEIGHT * 3, buf);
        }
        else
        {
            //draw pointer
            u8g2_DrawDisc(&u8g2, POINTER_RAD, ((menu_pointer % LINES_PER_PAGE) * LINE_HEIGHT + POINTER_RAD * 2),
                          POINTER_RAD, U8G2_DRAW_ALL);

            //draw names
            const u8g2_uint_t text_start_x = POINTER_RAD * 2 + 2;
            const u8g2_uint_t max_visible_width = u8g2_GetDisplayWidth(&u8g2) - text_start_x;

            for (uint8_t i = 0; i < LINES_PER_PAGE; i++)
            {
                const uint16_t name_index = (menu_pointer / LINES_PER_PAGE) * LINES_PER_PAGE + i;
                if (name_index >= recipes_count) break;

                bool is_selected = (name_index == menu_pointer);
                u8g2_uint_t text_width = u8g2_GetUTF8Width(&u8g2, names[name_index]);
                int16_t draw_x = text_start_x;

                // --- SCROLLING MATH ---
                if (is_selected && text_width > max_visible_width)
                {
                    const int16_t max_scroll = text_width - max_visible_width;

                    if (scroll_wait < DISPLAY_SCROLL_WAIT_MS)
                    {
                        scroll_wait += DISPLAY_REFRESH_TIME_MS; // Wait before scrolling
                    }
                    else
                    {
                        scroll_x += SCROLL_SPEED;
                        // Overshoot slightly, pause, then snap back to the beginning
                        if (scroll_x > max_scroll + 20)
                        {
                            scroll_x = 0;
                            scroll_wait = 0;
                        }
                    }

                    // Cap the visual shift so the text stops cleanly at its end before resetting
                    int16_t visual_scroll = (scroll_x > max_scroll) ? max_scroll : scroll_x;
                    draw_x -= visual_scroll;
                }

                u8g2_SetClipWindow(&u8g2, text_start_x, 0, 127, 63);
                u8g2_DrawUTF8(&u8g2, draw_x, i * LINE_HEIGHT, names[name_index]);
                u8g2_SetMaxClipWindow(&u8g2);
            }
        }
        if (xSemaphoreTake(display_i2c_mutex, portMAX_DELAY))
        {
            u8g2_SendBuffer(&u8g2);
            xSemaphoreGive(display_i2c_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_TIME_MS));
    }
}
