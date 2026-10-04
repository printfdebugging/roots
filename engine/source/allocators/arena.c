#include "engine/utils/logging.h"
#include "engine/allocators/arena.h"

#include <stdlib.h>

struct arena *arena_create(struct arena_options options) {
	struct arena *arena = calloc(1, sizeof(struct arena));
	if (!arena) {
		LOG_ERROR("failed to allocate memory for arena\n");
		return NULL;
	}

	arena->options = options;
	return arena;
}

void arena_destroy(struct arena *arena) {
	free(arena);
}
