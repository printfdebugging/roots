#ifndef TEXT_H
#define TEXT_H

#include "types.h"

/* note: todo: the text api has to change */
/* return string views into sub portions of the text */

struct Text;

struct Text *TextLoadFromFile(const char *filepath);

struct Text *TextLoadFromData(const char *data, u32 dataLength);

bool TextWriteToFile(const char *filepath);

u32 TextGetLineCount(struct Text *text);

char *TextGetUTF8Line(struct Text *text, u32 line);

void TextDestroy(struct Text *text);

u32 TextGetLineLength(struct Text *text, u32 lineIdx);

#endif
