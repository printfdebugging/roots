#include "utils/string.h"

#include <string.h>
#include <stdlib.h>

char *StringDuplicate(const char *str)
{
   uint64_t strLen = strlen(str);
   if (strLen == 0)
      return NULL;

   char *string = calloc(strLen + 1, sizeof(char));
   if (string == NULL)
      return string;

   strcpy(string, str);
   return string;
}
