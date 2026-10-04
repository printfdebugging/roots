#ifndef UTILS_POOL_H
#define UTILS_POOL_H

#include <stdint.h>

struct pool_options {
	uint32_t size;
};

struct pool {
	bool initialized;
	struct pool_options options;
};

struct pool *pool_create(struct pool_options options);
void pool_destroy(struct pool *pool);

#endif
