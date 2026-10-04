#include "engine/utils/logging.h"
#include "engine/allocators/pool.h"

#include <stdlib.h>

struct pool *pool_create(struct pool_options options) {
	struct pool *pool = calloc(1, sizeof(struct pool));
	if (!pool) {
		LOG_ERROR("failed to allocate memory for pool\n");
		return NULL;
	}

	pool->options = options;
	return pool;
}

void pool_destroy(struct pool *pool) {
	free(pool);
}
