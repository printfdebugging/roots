#include "engine/allocators/arena.h"
#include "engine/utils/logging.h"
#include "engine/platform/memory.h"

#include <stdlib.h>

struct arena *arena_create(uint64_t size) {
	struct arena *arena = NULL;

	if (!(arena = calloc(1, sizeof(struct arena)))) {
		LOG_ERROR("arena_create: Failed to allocate memory for arena\n");
		goto error;
	}

	void *chunk = platform_reserve_memory(size);
	if (!chunk)
		goto error;

	arena->initialized = true;
	arena->size = size;
	arena->used = 0;
	arena->chunk = chunk;
	arena->commit = chunk;
	return arena;

error:
	free(arena);
	return NULL;
}

void *arena_allocate(struct arena *arena, uint64_t size) {
	uint64_t available = arena->size - arena->used;
	if (available < size) {
		LOG_ERROR("arena_allocate: Arena ran out of memory\n");
		return NULL;
	}

	if (platform_commit_memory(arena->commit, size)) {
		void *chunk = arena->commit;
		arena->commit = (uint8_t *) arena->commit + size;
		arena->used += size;
		return chunk;
	}

	return NULL;
}

void arena_destroy(struct arena *arena) {
	platform_free_memory(arena->chunk, arena->size);
}
