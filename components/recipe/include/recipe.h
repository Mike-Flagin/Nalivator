//
// Created by mikes on 31.03.2026.
//

#ifndef NALIVATOR_RECIPE_H
#define NALIVATOR_RECIPE_H
#include <stdint.h>
#include "cJSON.h"

typedef struct
{
    uint16_t id;
    uint16_t amount;
} ingredient_t;

typedef struct
{
    char* name;
    ingredient_t* ingredients;
} recipe_t;

typedef struct
{
    uint32_t offset;
    uint32_t size;
} recipe_index_t;

cJSON* get_recipe_json(uint16_t id);

void rebuild_index();

#endif //NALIVATOR_RECIPE_H
