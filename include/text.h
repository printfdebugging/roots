#ifndef TEXT_H
#define TEXT_H

#include "types.h"

/* note: todo: the text api has to change */
/* return string views into sub portions of the text */

struct Text *textLoadFromFile(const char *filepath);
struct Text *textLoadFromData(const char *data, u32 dataLength);
bool textWriteToFile(const char *filepath);
u32 textGetLineCount(struct Text *text);
char *textGetUTF8Line(struct Text *text, u32 line);
void textDestroy(struct Text *text);
bool textMoveCursorUp(struct Text *text);
bool textMoveCursorDown(struct Text *text);
bool textMoveCursorLeft(struct Text *text);
bool textMoveCursorRight(struct Text *text);
u32 textGetCursorLine(struct Text *text);
u32 textGetCursorColumn(struct Text *text);

#endif
