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
      if (lineLen == 0)
         continue;

      text->lines                    = realloc(text->lines, (sizeof(char *)) * (text->lineCount + 1));
      text->lines[text->lineCount++] = line;
      line                           = NULL;
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
