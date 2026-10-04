#ifndef UTILS_ARENA_H
#define UTILS_ARENA_H

#include <stdint.h>

struct arena_options {
	uint32_t size;
};

struct arena {
	bool initialized;
	struct arena_options options;
};

struct arena *arena_create(struct arena_options options);
void arena_destroy(struct arena *arena);

#endif
