#include "glad/glad.h"

#include "editor.h"

int main(int argc, char *argv[])
{
   (void) argc;
   (void) argv;

   struct Editor editor = { 0 };
   if (!editorInit(&editor) || !editorRun(&editor) || !editorDeInit(&editor))
      return EXIT_FAILURE;
   return EXIT_SUCCESS;
}
