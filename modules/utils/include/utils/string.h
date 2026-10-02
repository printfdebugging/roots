#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include <stdint.h>

struct string {
	char *data;
	uint32_t count;
};

struct string_view {
	char *data;
	uint32_t count;
	struct string *source;
};

/**!
 * Duplicates the string, i.e. allocates memory for the bytes and a `\0`,
 * and then uses `strcpy` to copy the string to the allocated memory.
 *
 * The caller is responsible for managing the `lifetime` of the returned
 * string i.e. freeing it. Returns `NULL` on error.
 */
char *string_duplicate(const char *str);

/* todo: rename to a string helper  and return a String, with the length that is */
char *string_read_file_contents(const char *file_path);

#endif
