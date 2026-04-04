#include "tasks.h"

#include "recipe.h"

SemaphoreHandle_t recipes_mutex;
SemaphoreHandle_t ingredients_mutex;
SemaphoreHandle_t config_mutex;

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

void init_tasks()
{
    recipes_mutex = xSemaphoreCreateMutex();
    ingredients_mutex = xSemaphoreCreateMutex();
    config_mutex = xSemaphoreCreateMutex();
}
