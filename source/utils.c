#include <string.h>

#include "editor.h"

#include "stb_image.h"

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

GLFWimage *imageLoadFromFile(const char *filepath)
{
   GLFWimage *image = NULL;
   i32 imgChannels  = 0;

   if (!filepath)
      goto failure;
   if (!(image = calloc(1, sizeof(GLFWimage))))
      goto failure;
   if (!(image->pixels = stbi_load(filepath, &image->width, &image->height, &imgChannels, 0)))
      goto failure;

   return image;

failure:
   imageDestroy(image);
   return NULL;
}

void imageDestroy(GLFWimage *image)
{
   if (image)
      free(image->pixels);
   free(image);
}