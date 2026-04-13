//
// Created by mikes on 31.03.2026.
//
#include "recipe.h"
#include "esp_log.h"
#include "littlefs_helper.h"
#include "pumps.h"
#include "../../include/files.h"
#include "../../include/values.h"
const static char TAG[] = "Recipes";


cJSON* get_recipe_json(const uint16_t id)
{
    FILE* fidx = open_file_to_read_binary(RECIPES_INDEX_PATH);
    FILE* f = open_file_to_read(RECIPES_PATH);
    if (f == NULL || fidx == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", f == NULL ? RECIPES_PATH : RECIPES_INDEX_PATH);
        if (f != NULL) close_file(f);
        if (fidx != NULL) close_file(fidx);
        return NULL;
    }

    if (fseek(fidx, (id - 1) * sizeof(recipe_index_t), SEEK_SET)) // NOLINT(*-narrowing-conversions)
    {
        // if no id
        ESP_LOGE(TAG, "Failed to read id %d", id - 1);
        close_file(fidx);
        close_file(f);
        return NULL;
    }
    recipe_index_t entry;
    fread(&entry, sizeof(recipe_index_t), 1, fidx);

    char* buffer = malloc(entry.size + 1);
    if (!buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    fseek(f, entry.offset, SEEK_SET); // NOLINT(*-narrowing-conversions)
    fread(buffer, 1, entry.size, f);

    buffer[entry.size] = '\0';

    cJSON* root = cJSON_Parse(buffer);

    free(buffer);
    close_file(fidx);
    close_file(f);

    return root;
}

recipe_t get_recipe(const uint16_t id)
{
    cJSON* recipe_json = get_recipe_json(id);
    recipe_t recipe = {0};
    if (recipe_json == NULL)
    {
        return recipe;
    }

    recipe.id = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(recipe_json, RECIPE_ID_JSON_KEY));
    recipe.name = cJSON_GetStringValue(cJSON_GetObjectItem(recipe_json, RECIPE_NAME_JSON_KEY));
    const cJSON* ingredients_array = cJSON_GetObjectItem(recipe_json, RECIPE_INGREDIENTS_JSON_KEY);
    recipe.ingredients_amount = cJSON_GetArraySize(ingredients_array);
    recipe.ingredients = malloc(recipe.ingredients_amount * sizeof(ingredient_t));
    for (uint8_t i = 0; i < recipe.ingredients_amount; i++)
    {
        const cJSON* ingredient = cJSON_GetArrayItem(ingredients_array, i);
        recipe.ingredients[i].id = (uint16_t)cJSON_GetNumberValue(
            cJSON_GetObjectItem(ingredient, INGREDIENT_ID_JSON_KEY));
        recipe.ingredients[i].amount = (uint16_t)cJSON_GetNumberValue(
            cJSON_GetObjectItem(ingredient, INGREDIENT_AMOUNT_JSON_KEY));
    }
    cJSON_Delete(recipe_json);
    return recipe;
}

void rebuild_index()
{
    FILE* f = open_file_to_read(RECIPES_PATH);
    FILE* fidx = open_file_to_write_binary(RECIPES_INDEX_PATH);
    if (f == NULL || fidx == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", f == NULL ? RECIPES_PATH : RECIPES_INDEX_PATH);
        if (f != NULL) fclose(f);
        if (fidx != NULL) fclose(fidx);
        return;
    }

    char c;
    int brace_depth = 0;
    recipe_index_t current_recipe = {0};
    while (fread(&c, 1, 1, f) > 0)
    {
        if (c == '{')
        {
            if (brace_depth == 0)
            {
                current_recipe.offset = ftell(f) - 1;
            }
            brace_depth++;
        }
        else if (c == '}')
        {
            brace_depth--;
            if (brace_depth == 0)
            {
                current_recipe.size = ftell(f) - current_recipe.offset;
                fwrite(&current_recipe, sizeof(recipe_index_t), 1, fidx);
            }
        }
    }

    close_file(f);
    close_file(fidx);
    ESP_LOGI(TAG, "Index built");
}

bool check_recipe_id(const uint16_t id)
{
    FILE* fidx = open_file_to_read_binary(RECIPES_INDEX_PATH);
    if (fidx == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", RECIPES_INDEX_PATH);
        return false;
    }

    if (fseek(fidx, id * sizeof(recipe_index_t), SEEK_SET)) // NOLINT(*-narrowing-conversions)
    {
        // if no id
        ESP_LOGE(TAG, "Failed to read id %d", id);
        close_file(fidx);
        return false;
    }
    return true;
}

bool check_recipe(const recipe_t* recipe)
{
    for (uint8_t i = 0; i < recipe->ingredients_amount; i++)
    {
        for (uint8_t j = 0; j < PUMPS_AMOUNT; j++)
        {
            if (recipe->ingredients[i].id == pumps[j].ingredient_id)
            {
                break;
            }
            if (j < PUMPS_AMOUNT - 1) continue;
            return false;
        }
    }
    return true;
}
