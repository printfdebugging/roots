#include "viewer.h"

#include <stdlib.h>

int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	if (!viewer_init() || !viewer_run() || !viewer_deinit())
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
