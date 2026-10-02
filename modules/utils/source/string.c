#include "utils/string.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *string_duplicate(const char *str)
{
   uint64_t length = strlen(str);
   if (length == 0)
      return NULL;

   char *string = calloc(length + 1, sizeof(char));
   if (string == NULL)
      return string;

   strcpy(string, str);
   return string;
}

char *string_read_file_contents(const char *file_path)
{
   char *data = NULL;
   FILE *file = fopen(file_path, "rb");
   if (!file)
   {
      fprintf(stderr, "failed to read shader file: %s\n", file_path);
      return NULL;
   }

   fseek(file, 0, SEEK_END);
   int64_t length = ftell(file);
   fseek(file, 0, SEEK_SET);

   if (length < 0)
   {
      fprintf(stderr, "failed to get the shader file's length: %s\n", file_path);
      goto failure;
   }

   if (!(data = calloc(1, (uint32_t) length + 1)))
   {
      fprintf(stderr, "failed to allocate memory for data to store file %s\n", file_path);
      goto failure;
   }

   uint64_t read_count = fread(data, 1, (uint32_t) length, file);
   if (read_count < (uint32_t) length || read_count == 0)
   {
      fprintf(stderr, "read returned %llu which is either 0 or less than %lli", read_count, length);
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
