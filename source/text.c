#ifdef TEXT_LINE_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>

#include "editor.h"

struct Text
{
   char *filePath;
   char **lines;
   u32 lineCount;
};

struct Text *textLoadFromFile(const char *filepath)
{
   /*****************
    * read the file *
    ****************/
   FILE *file = NULL;
   if (!(file = fopen(filepath, "r")))
   {
      fprintf(stderr, "failed to open file: %s", filepath);
      return NULL;
   }

   struct Text *text = calloc(1, sizeof(struct Text));

   char *line  = NULL;
   u64 lineCap = 0;
   i32 lineLen = 0;
   while ((lineLen = (i32) getline(&line, &lineCap, file)) != -1)
   {
      while (lineLen > 1 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r'))
         lineLen--;
      if (lineLen == 0)
         continue;

      text->lines = realloc(text->lines, text->lineCount + 1);
      /****************************************************************************************
       * note: The substitution happens in the renderer where we just draw the visible lines. *
       * There we substitute tabs with #spaces & newlines with a space as well                *
       ***************************************************************************************/
      text->lines[text->lineCount++] = line;
   }

   fclose(file);
   return text;
}

bool textWriteToFile(const char *filepath)
{
   perror("todo");
   return true;
}

u32 textGetLineCount(struct Text *text)
{
   return text->lineCount;
}

char *textGetUTF8Line(struct Text *text, u32 line)
{
   if (text->lineCount < line)
      return NULL;
   return text->lines[line];
}

void textDestroy(struct Text *text)
{
   for (u32 lineIdx = 0; lineIdx < text->lineCount; ++lineIdx)
      free(text->lines[lineIdx]);
   free(text->lines);
}

#endif
