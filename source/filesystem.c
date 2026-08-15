#include <stdio.h>
#include <stdlib.h>

#include "editor.h"

char *readFileContents(const char *filPath)
{
   char *data = NULL;
   FILE *file = fopen(filPath, "rb");
   if (!file)
   {
      fprintf(stderr, "failed to read shader file: %s\n", filPath);
      return NULL;
   }

   fseek(file, 0, SEEK_END);
   i64 length = ftell(file);
   fseek(file, 0, SEEK_SET);

   if (length < 0)
   {
      fprintf(stderr, "failed to get the shader file's length: %s\n", filPath);
      goto failure;
   }

   if (!(data = calloc(1, (u32) length + 1)))
   {
      fprintf(stderr, "failed to allocate memory for data to store file %s\n", filPath);
      goto failure;
   }

   u64 readCount = fread(data, 1, (u32) length, file);
   if (readCount < (u32) length || readCount == 0)
   {
      fprintf(stderr, "read returned %li which is either 0 or less than %li", readCount, length);
      goto failure;
   }

   data[length] = '\0';
   if (fclose(file))
   {
      fprintf(stderr, "fclose failed\n");
      goto failure;
   }

   return data;

failure:
   fclose(file);
   free(data);
   return NULL;
}
