//
// Created by mikes on 31.03.2026.
//
#include "recipe.h"
#include "esp_log.h"
#include "littlefs_helper.h"
#include "../../include/files.h"
const static char TAG[] = "Recipes";


cJSON* get_recipe_json(const uint16_t id)
{
    FILE* fidx = open_file_to_read_binary(RECIPES_INDEX_PATH);
    FILE* f = open_file_to_read(RECIPES_PATH);
    if (f == NULL || fidx == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", f == NULL ? RECIPES_PATH : RECIPES_INDEX_PATH);
        if (f != NULL) fclose(f);
        if (fidx != NULL) fclose(fidx);
        return NULL;
    }

    fseek(fidx, id * sizeof(recipe_index_t), SEEK_SET); // NOLINT(*-narrowing-conversions)
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
