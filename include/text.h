#ifndef TEXT_H
#define TEXT_H

#include "types.h"

/*****************************************************************************
 * Opaque `Text` type. There would be a few implementations in the backend,  *
 * a `GapBuffer` implementation, a `Rope` implementation, and the user would *
 * be able to choose which implementation they want to use.                  *
 ****************************************************************************/
struct Text;

struct Text *textLoadFromFile(const char *filepath);
bool textWriteToFile(const char *filepath);
u32 textGetLineCount(struct Text *text);
char *textGetUTF8Line(struct Text *text, u32 line);
void textDestroy(struct Text *text);

#endif
