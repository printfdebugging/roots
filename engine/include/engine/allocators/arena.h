#ifndef UTILS_ARENA_H
#define UTILS_ARENA_H

#include <stdint.h>

struct arena {
	bool initialized;
	uint64_t size;
	uint64_t used;

	void *commit;
	void *chunk;
};

struct arena *arena_create(uint64_t size);
void *arena_allocate(struct arena *arena, uint64_t size);
void arena_destroy(struct arena *arena);

#endif
