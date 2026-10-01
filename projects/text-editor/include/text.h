#ifndef TEXT_H
#define TEXT_H

#include "types.h"

/* note: todo: the text api has to change */
/* return string views into sub portions of the text */

struct Text;

struct Text *TextLoadFromFile(const char *filepath);

struct Text *TextLoadFromData(const char *data, uint32_t dataLength);

bool TextWriteToFile(const char *filepath);

uint32_t TextGetLineCount(struct Text *text);

char *TextGetUTF8Line(struct Text *text, uint32_t line);

struct StringView TextGetLineUTF8AtOffset(struct Text *text, uint32_t line, uint32_t offset);

void TextDestroy(struct Text *text);

uint32_t TextGetLineLength(struct Text *text, uint32_t lineIdx);

#endif
