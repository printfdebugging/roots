#include "glad/glad.h"

#include "editor.h"

int main(int argc, char *argv[])
{
   (void) argc;
   (void) argv;

   struct Editor editor;
   editorRun(&editor);
   return 0;
}
