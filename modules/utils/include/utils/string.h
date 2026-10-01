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

char *StringDuplicate(const char *str);

#endif
