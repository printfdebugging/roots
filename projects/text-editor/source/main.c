#include "editor.h"

int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	if (!editor_init() || !editor_run() || !editor_deinit())
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
