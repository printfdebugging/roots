#ifdef TEXT_LINE_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "text.h"
#include "types.h"

struct Text
{
   char *filePath;
   struct cString *lines;
   u32 lineCount;
};

struct Text *TextLoadFromFile(const char *filepath)
{
   /**!
    * warning: `getline` is a UNIX only function, so can't
    * use that on Windows.
    */
#ifdef _WIN32
   /**!
    * fixme: strlen here can be avoided by returning a struct
    * from readFileContents, or by passing an out variable for
    * the data and returning the length.
    */
   char *data = readFileContents(filepath);
   if (!data)
      return NULL;

   struct Text *text = textLoadFromData(data, (u32) strlen(data));
   text->filePath = stringDuplicate(filepath);

   free(data);
   return text;

#else
   FILE *file = NULL;
   if (!(file = fopen(filepath, "r")))
   {
      fprintf(stderr, "failed to open file: %s", filepath);
      return NULL;
   }

   struct Text *text = calloc(1, sizeof(struct Text));

   char *line = NULL;
   u64 lineCap = 0;
   i32 lineLen = 0;
   while ((lineLen = (i32) getline(&line, &lineCap, file)) != -1)
   {
      if (lineLen == 0)
         continue;

      text->lines = realloc(text->lines, (sizeof(struct cString)) * (text->lineCount + 1));
      text->lines[text->lineCount++] = (struct cString) {
         .data = line,
         .count = (u32) lineLen,
      };
      line = NULL;
   }

   fclose(file);

   // note: The last getline returns an empty string "" at the end of the file.
   // So we should call free on that, or that leads to memory leaks.
   free(line);
   return text;
#endif
}

struct Text *TextLoadFromData(const char *data, u32 dataLength)
{
   if (data == NULL)
      perror("got null data");

   u32 index = 0;
   u32 lastIndex = 0;

   struct Text *text = calloc(1, sizeof(struct Text));
   if (!text) perror("failed to allcoate Text");

   while (index < dataLength)
   {
      if ((data[index++] != '\n') && index != dataLength)
         continue;

      u32 length = index - lastIndex;

      char *buffer = calloc(length + 1, sizeof(char));
      if (!buffer)
         perror("failed to allocate buffer\n");

      buffer[length] = '\0';
      buffer = memcpy(buffer, data + lastIndex, length);
      lastIndex = index;

      text->lines = realloc(text->lines, (text->lineCount + 1) * sizeof(struct cString));
      text->lines[text->lineCount++] = (struct cString) {
         .data = buffer,
         .count = length,
      };
   }

   return text;
}

bool TextWriteToFile(const char *filepath)
{
   (void) filepath;
   perror("todo");
   return true;
}

u32 TextGetLineCount(struct Text *text)
{
   return text->lineCount;
}

char *TextGetUTF8Line(struct Text *text, u32 line)
{
   if (text->lineCount <= line)
      return NULL;
   return text->lines[line].data;
}

void TextDestroy(struct Text *text)
{
   for (u32 lineIdx = 0; lineIdx < text->lineCount; ++lineIdx)
      free(text->lines[lineIdx].data);
   free(text->lines);
   free(text->filePath);
}

u32 TextGetLineLength(struct Text *text, u32 lineIdx)
{
   if (lineIdx <= text->lineCount - 1)
      return text->lines[lineIdx].count;
   return 0;
}

#endif
