#include <stdio.h>
#include <stdlib.h>

#include "filesystem.h"
#include "types.h"

char *ReadFileContents(const char *filPath)
{
   char *data = NULL;
   FILE *file = fopen(filPath, "rb");
   if (!file)
   {
      fprintf(stderr, "failed to read shader file: %s\n", filPath);
      return NULL;
   }

   fseek(file, 0, SEEK_END);
   int64_t length = ftell(file);
   fseek(file, 0, SEEK_SET);

   if (length < 0)
   {
      fprintf(stderr, "failed to get the shader file's length: %s\n", filPath);
      goto failure;
   }

   if (!(data = calloc(1, (uint32_t) length + 1)))
   {
      fprintf(stderr, "failed to allocate memory for data to store file %s\n", filPath);
      goto failure;
   }

   uint64_t readCount = fread(data, 1, (uint32_t) length, file);
   if (readCount < (uint32_t) length || readCount == 0)
   {
      fprintf(stderr, "read returned %llu which is either 0 or less than %lli", readCount, length);
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
