#include <string.h>

#include "editor.h"

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
