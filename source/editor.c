#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"

#include "editor.h"

void _glfwErrFn(int code, const char *description);

/* next: create editor API to operate over the ID rather than having to
 * -> and then index with ID again and agian, that's unsafe.
 */
bool editorRun(struct Editor *editor)
{
   i32 bufId = editorOpenFile(editor, ASSETS_DIR "test.md");

   while (!editorShouldClose(editor))
   {
      editorCalcFrameTime(editor);
      glfwPollEvents();

      editorDrawBuffer(editor, bufId);
   }

   return true;
}

bool editorShouldClose(struct Editor *editor)
{
   if (!editor->initialized)
      return true;

   bool shouldClose = true;
   for (i32 winId = 0; winId < (i32) editor->windowCount; ++winId)
      if (winId != editor->sharedWindowId)
         shouldClose &= glfwWindowShouldClose(editor->window[winId]);

   return shouldClose;
}

bool editorInit(struct Editor *editor)
{
   if (editor->initialized)
      return true;

   editor->fontSize = DEFAULT_FONT_SIZE;
   if (!(editor->fontFilePath = stringDuplicate(DEFAULT_FONT_FILE_PATH)))
      return false;

   editor->sharedWindowId = editorCreateWindow(
       editor,
       (struct GLFWwindowOptions) {
          .width       = 800,
          .height      = 600,
          .title       = "GLFWwindow",
          .transparent = false,
          .visible     = false,
          .fbResizeFn  = windowResize,
          .keyFn       = keyPress,
          .userdata    = editor,
       }
   );

   glfwSetErrorCallback(_glfwErrFn);

   // fontManagerInit(editor->fontFilePath);
   if (!(editor->lineShader = calloc(1, sizeof(struct LineShader))))
      return false;
   // lineShaderInit(editor->lineShader);

   editor->initialized = true;
   return true;
}

void editorCalcFrameTime(struct Editor *editor)
{
   f64 timeNow       = glfwGetTime();
   editor->timeDelta = timeNow - editor->lastTime;
   editor->lastTime  = timeNow;
}

bool editorDeInit(struct Editor *editor)
{
   for (u32 idx = 0; idx < editor->textCount; ++idx)
      textDestroy(editor->text[idx]);
   for (u32 idx = 0; idx < editor->textCount; ++idx)
      free(editor->text[idx]);

   for (u32 idx = 0; idx < editor->lineRendererCount; ++idx)
      lineRendererDeInit(editor->lineRenderer[idx]);
   for (u32 idx = 0; idx < editor->lineRendererCount; ++idx)
      free(editor->lineRenderer[idx]);

   for (u32 idx = 0; idx < editor->bufferCount; ++idx)
   {
      free(editor->textBuffer[idx]->visLineRenderers);
      free(editor->textBuffer);
   }

   /**!
    * note: Till we have a shared hidden window which
    * is destroyed at the end, we need to do this last
    */
   for (u32 idx = 0; idx < editor->windowCount; ++idx)
      glfwDestroyWindow(editor->window[idx]);

   /* note: todo: maybe this should be above the window destruction sequence */
   lineShaderDeInit(editor->lineShader);
   fontManagerDeInit();

   free(editor->text);
   free(editor->window);
   free(editor->lineRenderer);
   free(editor->lineShader);
   free(editor->fontFilePath);

   return true;
}

/**!
 * Loads the text file from `filePath` into a `Text` object,
 * and returns an index to it, or `-1` on error.
 */
i32 editorLoadTextFile(struct Editor *editor, const char *filePath)
{
   if (!filePath)
      return -1;

   struct Text *text = textLoadFromFile(filePath);
   if (!text)
      return -1;

   editor->text = realloc(editor->text, sizeof(struct Text *) * (editor->textCount + 1));
   if (!editor->text)
      return -1;

   editor->text[editor->textCount] = text;
   return (i32) editor->textCount++;
}

i32 editorCreateWindow(struct Editor *editor, struct GLFWwindowOptions opts)
{
   GLFWwindow *window = windowCreate(opts);
   if (!window)
      return -1;

   editor->window = realloc(editor->window, sizeof(GLFWwindow *) * (editor->windowCount + 1));
   if (!editor->window)
      return -1;

   editor->window[editor->windowCount] = window;
   return (i32) editor->windowCount++;
}

/**!
 * A sane argument against this per line approach is to do it for
 * all the visible lines at the same time. That makes sense, though
 * is not actionable at the moment as that would require doing many
 * things at the same time.. so listing that as a todo: here.
 *
 * For now this works because we would have to relayout each line as they
 * are marked dirty and there are less things to manage here.. But as
 * it gets in shape, we would move these to one large function, maybe...
 * intuition says we would still need to keep the per line thing..
 */
i32 editorCreateLine(struct Editor *editor, struct LineOptions opts)
{
   if (!editor->lineShader)
      return -1;
   if (opts.textId == -1 /*  && !opts.isVirtual */)
      return -1;

   struct LineRenderer *renderer = calloc(1, sizeof(struct LineRenderer));
   if (!renderer)
      return -1;

   lineRendererInit(renderer, editor->lineShader);

   /* todo: hide strlen behind the text api so that we can later replace it with something more efficient. */
   struct Text *text = editor->text[opts.textId];
   char *lineBytes   = textGetUTF8Line(text, opts.lineIdx);
   u64 lineByteLen   = strlen(lineBytes);
   fontManagerLayoutLine(renderer, lineBytes, lineByteLen);

   if (!(editor->lineRenderer = realloc(editor->lineRenderer, sizeof(struct LineRenderer *) * (editor->lineRendererCount + 1))))
   {
      lineRendererDeInit(renderer);
      free(renderer);
      return -1;
   }

   editor->lineRenderer[editor->lineRendererCount] = renderer;
   return (i32) editor->lineRendererCount++;
}

void _glfwErrFn(int code, const char *description)
{
   fprintf(stderr, "_glfwErrFun: code: %i, msg: %s\n", code, description);
}
