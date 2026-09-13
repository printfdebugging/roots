#include "glad/glad.h"

#include "editor.h"

int main(int argc, char *argv[])
{
   (void) argc;
   (void) argv;

   if (!editorInit() || !editorRun() || !editorDeInit())
      return EXIT_FAILURE;
   return EXIT_SUCCESS;
}
