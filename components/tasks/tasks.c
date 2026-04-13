#include "tasks.h"
#include "buttons.h"
#include "esp_log.h"
#include "littlefs_helper.h"
#include "recipe.h"
#include "servo.h"
#include "../../include/pins.h"

TaskHandle_t pour_task_handle;
TaskHandle_t servo_task_handle;

SemaphoreHandle_t recipes_mutex;
SemaphoreHandle_t ingredients_mutex;
SemaphoreHandle_t config_mutex;
SemaphoreHandle_t servo_mutex;

static const char* TAG = "tasks";

static TickType_t last_button_time[4] = {0, 0, 0, 0};

config_t config;

enum glass_state glasses[GLASSES_AMOUNT] = {POURED};

void print_cpu_load(void* pvParameters)
{
    while (true)
    {
        // Allocate a buffer to hold the output string
        char* stats_buffer = malloc(1024);

        // This function populates the buffer with the CPU stats table
        vTaskGetRunTimeStats(stats_buffer);

        // Print the result to the console
        ESP_LOGI("CPU", "Task Execution Stats:\n%s", stats_buffer);
        free(stats_buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void load_config(const char* path)
{
    config.pumps = pumps;
    ESP_LOGI(TAG, "Attempting to load config: %s", path);
    if (config_mutex == NULL) printf("CRITICAL: Mutex is NULL!\n");

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
                                cJSON_GetNumberValue(cJSON_GetObjectItem(p, INGREDIENT_ID_JSON_KEY));
                            volumes_to_splitter[i] = (uint8_t)cJSON_GetNumberValue(
                                cJSON_GetObjectItem(p, VOLUME_TO_SPLITTER_JSON_KEY));
                        }

                        configure_pumps(flowrates, inverses, ingredients, volumes_to_splitter);


                        // Parse servoPositions array
                        const cJSON* servo_positions_json = cJSON_GetObjectItem(root, SERVO_POSITIONS_JSON_KEY);
                        for (int i = 0; i < GLASSES_AMOUNT; i++)
                        {
                            config.servo_positions[i] = (uint16_t)cJSON_GetNumberValue(
                                cJSON_GetArrayItem(servo_positions_json, i));
                        }

                        // Parse volumeAfterSplitter
                        const cJSON* volume_after_splitter_json = cJSON_GetObjectItem(
                            root, VOLUME_AFTER_SPLITTER_JSON_KEY);
                        config.volume_after_splitter = (uint16_t)cJSON_GetNumberValue(volume_after_splitter_json);

                        // Parse LED colors arrays
                        const cJSON* standby_color_json = cJSON_GetObjectItem(root, LED_STANDBY_COLOR_JSON_KEY);
                        const color_t standby = {
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(standby_color_json, 0)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(standby_color_json, 1)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(standby_color_json, 2))
                        };
                        config.led_standby = standby;

                        const cJSON* waiting_color_json = cJSON_GetObjectItem(root, LED_WAITING_COLOR_JSON_KEY);
                        const color_t waiting = {
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(waiting_color_json, 0)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(waiting_color_json, 1)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(waiting_color_json, 2))
                        };
                        config.led_waiting = waiting;

                        const cJSON* pouring_color_json = cJSON_GetObjectItem(root, LED_POURING_COLOR_JSON_KEY);
                        const color_t pouring = {
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(pouring_color_json, 0)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(pouring_color_json, 1)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(pouring_color_json, 2))
                        };
                        config.led_pouring = pouring;

                        const cJSON* poured_color_json = cJSON_GetObjectItem(root, LED_POURED_COLOR_JSON_KEY);
                        const color_t poured = {
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(poured_color_json, 0)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(poured_color_json, 1)),
                            (uint8_t)cJSON_GetNumberValue(cJSON_GetArrayItem(poured_color_json, 2))
                        };
                        config.led_poured = poured;

                        config.current_recipe = (recipe_t){0};
                        config.portion = 1;
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
            load_config(DEFAULT_CONFIG_PATH);
        }
        else
        {
            ESP_LOGE(TAG, "Default configuration failed to load!");
        }
    }
}

void init_tasks()
{
    recipes_mutex = xSemaphoreCreateMutex();
    ingredients_mutex = xSemaphoreCreateMutex();
    config_mutex = xSemaphoreCreateMutex();
}

void rebuild_index_task(void* pvParameters)
{
    if (xSemaphoreTake(recipes_mutex, portMAX_DELAY))
    {
        rebuild_index();

        xSemaphoreGive(recipes_mutex);
    }

    vTaskDelete(NULL);
}

void restart_esp(void* arg)
{
    esp_restart();
}

void load_config_task(void* pvParameters)
{
    if (xSemaphoreTake(config_mutex, portMAX_DELAY))
    {
        load_config(CONFIG_PATH);
        xSemaphoreGive(config_mutex);
    }
    vTaskDelete(NULL);
}

void button_handler_task(void* pvParameters)
{
    button_event_t event;
    while (1)
    {
        // Wait forever until a pin number arrives in the queue
        if (xQueueReceive(button_queue, &event, portMAX_DELAY))
        {
            // Determine pointer of this pin
            int pointer;
            switch (event.pin_num)
            {
            case KEY_1_PIN:
                pointer = 0;
                break;
            case KEY_2_PIN:
                pointer = 1;
                break;
            case KEY_3_PIN:
                pointer = 2;
                break;
            case KEY_4_PIN:
                pointer = 3;
                break;
            default:
                continue;
            }

            const TickType_t now = xTaskGetTickCount();
            if (now - last_button_time[pointer] < pdMS_TO_TICKS(60))
            {
                // It hasn't been 60ms since THIS specific button changed. Ignore the bounce.
                continue;
            }
            last_button_time[pointer] = now;

            if (event.state)
            {
                ESP_LOGI(TAG, "Button %lu PRESSED DOWN", event.pin_num);
                glasses[pointer] = WAITING;
            }
            else
            {
                ESP_LOGI(TAG, "Button %lu RELEASED", event.pin_num);

                // If the glass we just removed is the one the machine is currently targeting
                if (pour_task_handle != NULL && glasses[pointer] == POURING)
                {
                    ESP_LOGW(TAG, "Active glass removed! Aborting pour!");
                    xTaskNotify(pour_task_handle, SIGNAL_ABORT, eSetBits);
                }
                glasses[pointer] = NOT_PRESENT;
            }
        }
    }
}

void servo_task(void* pvParameters)
{
    uint32_t target_angle;

    while (1)
    {
        // Sleep forever until pour_task sends us an angle
        xTaskNotifyWait(0x00, ULONG_MAX, &target_angle, portMAX_DELAY);

        ESP_LOGI(TAG, "Moving to position: %lu", target_angle);

        set_servo_position(target_angle);

        // Wait for the motor to finish turning
        vTaskDelay(pdMS_TO_TICKS(1500));

        // Handshake: Tell the pour_task that moving servo is finished
        if (pour_task_handle != NULL)
        {
            // Send the "Done" bit to the pour task
            xTaskNotify(pour_task_handle, SIGNAL_SERVO_DONE, eSetBits);
        }
    }
}

void leds_task(void* pvParameters)
{
    color_t last_leds[GLASSES_AMOUNT] = {0};
    while (1)
    {
        color_t current_leds[GLASSES_AMOUNT];
        bool needs_update = false;

        for (int i = 0; i < GLASSES_AMOUNT; i++)
        {
            switch (glasses[i])
            {
            case WAITING:
                current_leds[i] = config.led_waiting;
                break;
            case POURING:
                current_leds[i] = config.led_pouring;
                break;
            case POURED:
                current_leds[i] = config.led_poured;
                break;
            case NOT_PRESENT:
            default:
                current_leds[i] = config.led_standby;
                break;
            }

            if (current_leds[i].r != last_leds[i].r ||
                current_leds[i].g != last_leds[i].g ||
                current_leds[i].b != last_leds[i].b)
            {
                needs_update = true;
                last_leds[i] = current_leds[i]; // Update our history
            }
        }
        if (needs_update)
        {
            update_leds(current_leds);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void pour_task(void* pvParameters)
{
    while (1)
    {
        // Wait for a recipe to be selected
        if (config.current_recipe.id == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // --- RECIPE SELECTED ---
        // Wait for glasses
        bool any_glass_ready = false;
        while (!any_glass_ready)
        {
            // Allow the user to cancel the recipe while waiting
            if (config.current_recipe.id == 0) break;

            for (int i = 0; i < GLASSES_AMOUNT; i++)
            {
                if (glasses[i] == WAITING)
                {
                    any_glass_ready = true;
                    glasses[i] = POURING;
                    break;
                }
            }

            if (!any_glass_ready)
            {
                // Sleep for 100ms and check the buttons again
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        // If recipe was canceled while waiting, go back to idle
        if (config.current_recipe.id == 0)
        {
            continue;
        }

        // Iterate through all glasses
        for (int current_glass = 0; current_glass < GLASSES_AMOUNT; current_glass++)
        {
            // Skip this position if glass is not selected
            if (glasses[current_glass] != POURING)
            {
                continue;
            }

            // Clear out any old signals
            xTaskNotifyWait(0xFFFFFFFF, 0xFFFFFFFF, NULL, 0);

            // Wake up the servo task and send it to the current glass position
            if (servo_task_handle != NULL)
            {
                xTaskNotify(servo_task_handle, config.servo_positions[current_glass], eSetValueWithOverwrite);
            }

            // Wait for the Servo OR an Abort
            uint32_t received_signals = 0;
            xTaskNotifyWait(0x00, ULONG_MAX, &received_signals, portMAX_DELAY);

            if (received_signals & SIGNAL_ABORT)
            {
                ESP_LOGW(TAG, "Glass %d removed while servo was moving!", current_glass);
                // Move on to the next glass in the loop
                continue;
            }

            // Servo is in position, glass is still there. Start pouring.
            bool pour_aborted = false;

            for (int i = 0; i < config.current_recipe.ingredients_amount; i++)
            {
                uint32_t pour_time_ms = 0;

                // Turn on pump
                uint8_t current_pump = 0;
                for (int j = 0; j < PUMPS_AMOUNT; j++)
                {
                    if (config.current_recipe.ingredients[i].id == pumps[j].ingredient_id)
                    {
                        current_pump = j;
                        pour_time_ms = GET_POURING_TIME_MS(config.current_recipe.ingredients[i].amount, pumps[current_pump].flowrate);
                        enable_pump(&pumps[j], FORWARD);
                        break;
                    }
                }

                received_signals = 0;
                const BaseType_t timeout_result = xTaskNotifyWait(0x00, ULONG_MAX, &received_signals,
                                                                  pdMS_TO_TICKS(pour_time_ms));

                // Turn off pump
                disable_pump(&pumps[current_pump]);

                if (timeout_result == pdPASS && received_signals & SIGNAL_ABORT)
                {
                    ESP_LOGW(TAG, "Glass %d removed during pour!", current_glass);
                    pour_aborted = true;
                }

                // Stop motor and let magnetic field collapse
                disable_pump(&pumps[current_pump]);
                vTaskDelay(pdMS_TO_TICKS(50));

                // Reverse
                const uint32_t reverse_time_ms = GET_POURING_TIME_MS(config.volume_after_splitter, pumps[current_pump].flowrate);

                enable_pump(&pumps[current_pump], BACKWARD);

                // Use NotifyWait instead of vTaskDelay so we can catch (and clear) an abort if
                // the user pulls the glass away exactly while the machine is sucking back.
                uint32_t stray_signals = 0;
                xTaskNotifyWait(0x00, ULONG_MAX, &stray_signals, pdMS_TO_TICKS(reverse_time_ms));

                if (stray_signals & SIGNAL_ABORT)
                {
                    pour_aborted = true;
                }

                // Final Stop
                disable_pump(&pumps[current_pump]);

                // Wait for DC motors to calm down before starting the next pump
                vTaskDelay(pdMS_TO_TICKS(50));

                // If the drink was aborted at any point, break out of the ingredient loop
                if (pour_aborted)
                {
                    break;
                }
            }

            if (!pour_aborted)
            {
                glasses[current_glass] = POURED;
                ESP_LOGI(TAG, "Glass %d finished!", current_glass);
            }
        }
    }
}
