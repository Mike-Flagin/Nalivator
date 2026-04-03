#ifndef HELLO_WORLD_LITTLEFS_HELPER_H
#define HELLO_WORLD_LITTLEFS_HELPER_H
#include <stdio.h>
#include <stdbool.h>

void littlefs_mount();
FILE* open_file_to_read(const char* file_name);
FILE* open_file_to_write(const char* file_name);
bool delete_file(const char* file_name);
bool rename_file(const char* file_name, const char* new_file_name);
void close_file(FILE* file);

#endif //HELLO_WORLD_LITTLEFS_HELPER_H
