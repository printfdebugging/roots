#ifndef TYPES_CONTAINERS_H
#define TYPES_CONTAINERS_H

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

struct color {
	float r;
	float g;
	float b;
	float a;
};

#endif
