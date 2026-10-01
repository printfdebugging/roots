#ifdef TEXT_LINE_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "text.h"
#include "types.h"
#include "utils.h"
#include "filesystem.h"

struct Text
{
   char *filePath;
   struct String *lines;
   uint32_t lineCount;
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
    * from ReadFileContents, or by passing an out variable for
    * the data and returning the length.
    */
   char *data = ReadFileContents(filepath);
   if (!data)
      return NULL;

   struct Text *text = TextLoadFromData(data, (uint32_t) strlen(data));
   text->filePath = StringDuplicate(filepath);

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
   uint64_t lineCap = 0;
   int32_t lineLen = 0;
   while ((lineLen = (int32_t) getline(&line, &lineCap, file)) != -1)
   {
      if (lineLen == 0)
         continue;

      text->lines = realloc(text->lines, (sizeof(struct String)) * (text->lineCount + 1));
      text->lines[text->lineCount++] = (struct String) {
         .data = line,
         .count = (uint32_t) lineLen,
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

struct Text *TextLoadFromData(const char *data, uint32_t dataLength)
{
   if (data == NULL)
      perror("got null data");

   uint32_t index = 0;
   uint32_t lastIndex = 0;

   struct Text *text = calloc(1, sizeof(struct Text));
   if (!text) perror("failed to allcoate Text");

   while (index < dataLength)
   {
      if ((data[index++] != '\n') && index != dataLength)
         continue;

      uint32_t length = index - lastIndex;

      char *buffer = calloc(length + 1, sizeof(char));
      if (!buffer)
         perror("failed to allocate buffer\n");

      buffer[length] = '\0';
      buffer = memcpy(buffer, data + lastIndex, length);
      lastIndex = index;

      text->lines = realloc(text->lines, (text->lineCount + 1) * sizeof(struct String));
      text->lines[text->lineCount++] = (struct String) {
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

uint32_t TextGetLineCount(struct Text *text)
{
   return text->lineCount;
}

char *TextGetUTF8Line(struct Text *text, uint32_t line)
{
   if (text->lineCount <= line)
      return NULL;
   return text->lines[line].data;
}

struct StringView TextGetLineUTF8AtOffset(struct Text *text, uint32_t line, uint32_t offset)
{
   uint32_t length = TextGetLineLength(text, line);
   if (offset < length)
   {
      return (struct StringView) {
         .data = &text->lines[line].data[offset],
         .count = length - offset,
         .source = &text->lines[line],
      };
   }

   return (struct StringView) {};
}

void TextDestroy(struct Text *text)
{
   for (uint32_t lineIdx = 0; lineIdx < text->lineCount; ++lineIdx)
      free(text->lines[lineIdx].data);
   free(text->lines);
   free(text->filePath);
}

uint32_t TextGetLineLength(struct Text *text, uint32_t lineIdx)
{
   if (lineIdx <= text->lineCount - 1)
      return text->lines[lineIdx].count;
   return 0;
}

#endif
