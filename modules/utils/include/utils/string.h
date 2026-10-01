#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include <stdint.h>

struct String
{
   char *data;
   uint32_t count;
};

struct StringView
{
   char *data;
   uint32_t count;
   struct String *source;
};

/**!
 * Duplicates the string, i.e. allocates memory for the bytes and a `\0`,
 * and then uses `strcpy` to copy the string to the allocated memory.
 *
 * The caller is responsible for managing the `lifetime` of the returned
 * string i.e. freeing it. Returns `NULL` on error.
 */
char *StringDuplicate(const char *str);

/* todo: rename to a string helper  and return a String, with the length that is */
char *ReadFileContents(const char *filPath);

#endif
