#ifndef TOOLKIT_H
#define TOOLKIT_H

#include <stdint.h>

struct toolkit_options {
	const char *font_path;
	uint32_t font_size;
};

struct toolkit {
	bool initialized;
	struct toolkit_options options;
};

struct toolkit *toolkit_create(struct toolkit_options options);

void toolkit_destroy(struct toolkit *toolkit);

#endif
