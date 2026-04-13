//
// Created by mikes on 31.03.2026.
//

#ifndef NALIVATOR_RECIPE_H
#define NALIVATOR_RECIPE_H
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

typedef struct
{
    uint16_t id;
    uint16_t amount;
} ingredient_t;

typedef struct
{
    uint16_t id;
    char* name;
    ingredient_t* ingredients;
    uint8_t ingredients_amount;
} recipe_t;

typedef struct
{
    uint32_t offset;
    uint32_t size;
} recipe_index_t;

cJSON* get_recipe_json(uint16_t id);
recipe_t get_recipe(uint16_t id);
void rebuild_index();
bool check_recipe_id(uint16_t id);
bool check_recipe(const recipe_t* recipe);

#endif //NALIVATOR_RECIPE_H
