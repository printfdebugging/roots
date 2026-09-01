#ifdef TEXT_LINE_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "editor.h"

struct Text
{
   char *filePath;
   char **lines;
   u32 lineCount;

   /**!
    * note: It is assumed that whatever chanes these variables
    * takes them from one consistent state to another.. they
    * should not be changed directly.
    */
   u32 cursorLine;
   /**!
    * note: this later becomes the offset by character, not by
    * byte, but then we also need to see where we encode this
    * info in the gui code and if glyphs there map to the same indexing
    * as these characters.. or what to do in case where multiple
    * codepoints combine to become one glyph.. interesting
    * problem indeed.
    */
   u32 cursorColumn;
};

struct Text *textLoadFromFile(const char *filepath)
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
   return textLoadFromData(data, (u32) strlen(data));
   /* todo: set filepath before returning.. */

#else
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

   // note: The last getline returns an empty string "" at the end of the file.
   // So we should call free on that, or that leads to memory leaks.
   free(line);
   return text;
#endif
}

struct Text *textLoadFromData(const char *data, u32 dataLength)
{
   if (data == NULL)
      perror("got null data");

   u32 index     = 0;
   u32 lastIndex = 0;

   struct Text *text = calloc(1, sizeof(struct Text));
   if (!text) perror("failed to allcoate Text");
   text->filePath  = NULL;
   text->lines     = NULL;
   text->lineCount = 0;

   while (index < dataLength)
   {
      if ((data[index++] != '\n') && index != dataLength)
         continue;

      u32 length = index - lastIndex;

      char *buffer = calloc(length + 1, sizeof(char));
      if (!buffer)
         perror("failed to allocate buffer\n");

      buffer[length] = '\0';
      buffer         = memcpy(buffer, data + lastIndex, length);
      lastIndex      = index;

      text->lines                  = realloc(text->lines, (text->lineCount + 1) * sizeof(char *));
      text->lines[text->lineCount] = buffer;
      text->lineCount++;
   }

   text->cursorLine   = 0;
   text->cursorColumn = 0;
   return text;
}

bool textWriteToFile(const char *filepath)
{
   (void) filepath;
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

bool textMoveCursorUp(struct Text *text)
{
   /* already on the first line */
   if (text->cursorLine == 0)
      return false;

   /* decrement the line */
   --text->cursorLine;

   /* check if the line has cursorColumn */
   const char *line = text->lines[text->cursorLine];
   u64 lineLen      = strlen(line);

   /* if not move it to the last column of that line. */
   if (lineLen < text->cursorColumn)
      text->cursorColumn = (u32) lineLen - 1;

   return true;
}

bool textMoveCursorDown(struct Text *text)
{
   /* already on the last line */
   if (text->cursorLine == text->lineCount - 1)
      return false;

   const char *line = text->lines[text->cursorLine];
   u64 lineLen      = strlen(line);

   if (lineLen < text->cursorColumn)
      text->cursorColumn = (u32) lineLen - 1;

   return true;
}

bool textMoveCursorLeft(struct Text *text)
{
   if (text->cursorColumn != 0)
   {
      --text->cursorColumn;
      return true;
   }

   return false;
}

bool textMoveCursorRight(struct Text *text)
{
   const char *line = text->lines[text->cursorLine];
   u64 lineLen      = strlen(line);
   if (text->cursorColumn < lineLen - 1)
   {
      ++text->cursorColumn;
      return true;
   }

   return false;
}

u32 textGetCursorLine(struct Text *text)
{
   return text->cursorLine;
}

u32 textGetCursorColumn(struct Text *text)
{
   return text->cursorColumn;
}

#endif
