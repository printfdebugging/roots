#ifndef UTILS_CONTAINERS_H
#define UTILS_CONTAINERS_H

#include <stdint.h>

struct point {
	double x;
	double y;
};

struct rectangle {
	int64_t x;
	int64_t y;
	int64_t w;
	int64_t h;
};

#endif
