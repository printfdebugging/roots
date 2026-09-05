#include <string.h>

#include "editor.h"

/**!
 * Duplicates the string, i.e. allocates memory for the bytes and a `\0`,
 * and then uses `strcpy` to copy the string to the allocated memory.
 *
 * The caller is responsible for managing the `lifetime` of the returned
 * string i.e. freeing it. Returns `NULL` on error.
 */
char *stringDuplicate(const char *str)
{
   u64 strLen = strlen(str);
   if (strLen == 0)
      return NULL;

   char *string = calloc(strLen + 1, sizeof(char));
   if (string == NULL)
      return string;

   strcpy(string, str);
   return string;
}
