#ifndef EDITOR_H
#define EDITOR_H

#include "types.h"
#include "unicode/unicode.h"

struct Editor
{
   /*****************************************
    * editor - internal text representation *
    ****************************************/
   u32 lineBytelen;
   u32 lineRunelen;
   rune *lineRunes;

   /***************************
    * editor - internal state *
    **************************/
   i32 xScrollOffset;
   i32 cursorCol;
   i32 cursorOffset;

   /***********************
    * editor - core state *
    **********************/
   f32 fontSize;
   f32 displayDPI;
   char *fontFilePath;
};

void editorInit(struct Editor *editor);
void editorDeInit(struct Editor *editor);

#endif
