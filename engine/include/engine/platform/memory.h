#ifndef PLATFORM_MEMORY_H
#define PLATFORM_MEMORY_H

#include <stdint.h>

void *platform_reserve_memory(uint64_t size);
bool platform_commit_memory(void *chunk, uint64_t size);
void platform_free_memory(void *chunk, uint64_t size);
int64_t platform_get_page_size();

#endif
