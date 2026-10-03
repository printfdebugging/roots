#include "toolkit/toolkit.h"

#include <stdlib.h>

struct toolkit *toolkit_create(struct toolkit_options options) {
	(void) options;
	return NULL;
}

void toolkit_destroy(struct toolkit *toolkit) {
	(void) toolkit;
}
