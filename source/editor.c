#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"

#include "editor.h"

void _glfwErrFn(int code, const char *description);

bool editorRun(struct Editor *editor)
{
   GLFWwindow *sharedWindow = NULL;
   if (editor->sharedWindowId != -1)
      sharedWindow = editor->window[editor->sharedWindowId];

   i32 windowId = editorCreateWindow(
       editor,
       (struct GLFWwindowOptions) {
          .width       = 800,
          .height      = 600,
          .title       = "GLFWwindow",
          .transparent = true,
          .visible     = true,
          .fbResizeFn  = windowResize,
          .keyFn       = keyPress,
          .userdata    = editor,
          .shared      = sharedWindow,
       }
   );

   GLFWwindow *window = editor->window[windowId];

   /* todo: move these to editorInit */
   fontManagerInit(editor->fontFilePath);
   lineShaderInit(editor->lineShader);

   i32 textId = editorLoadTextFile(editor, ASSETS_DIR "test.md");

   struct Text *text = editor->text[textId];
   u32 lineCount     = textGetLineCount(text);

   i32 *lineRenderers = calloc(lineCount, sizeof(i32));
   if (!lineRenderers)
      perror("failed to allocate memory\n");
   memset(lineRenderers, -1, sizeof(i32) * lineCount);

   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      lineRenderers[lineIdx] = editorCreateLine(
          editor,
          (struct LineOptions) {
             .lineIdx = lineIdx,
             .textId  = textId,
          }
      );
   }

   /**!
    * note:
    * properly define a data flow pipeline, it exists
    * but as of now is very loosely defined. this doesn't mean
    * create fancy abstractions, just keep in check what happens when..
    */
   while (!editorShouldClose(editor))
   {
      editorCalcFrameTime(editor);

      i32 windowWidth, windowHeight;
      glfwGetWindowSize(window, &windowWidth, &windowHeight);

      /**!
       * note:
       * This might change when we get more than one windows,
       * how, we are not sure yet, would be able to tell only
       * when the time comes.
       *
       * We would be sharing OpenGL context between the GLFW
       * windows, possibly threading as well, so just the event
       * polling would happen here and the rest key callbacks
       * into their own modules called by each window via function
       * pointers..
       */
      glfwPollEvents();
      if (glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
         glfwSetWindowShouldClose(window, GLFW_TRUE);

      /**!
       * note:
       * calculate the line height and then use that to
       * count the number of visible lines and then render
       * those..
       */
      i32 xScale, yScale;
      struct Font *font = fontManagerGetDefaultFont();
      hb_font_get_scale(font->hbFont, &xScale, &yScale);
      f32 fontScale = editor->fontSize / (f32) yScale;

      struct GlyphAtlas *atlas = fontManagerGetGlyphAtlas();

      mat4s mvp = { GLM_MAT4_IDENTITY_INIT };
      mvp       = glms_ortho(0, (f32) windowWidth, 0, (f32) windowHeight, 0.0f, 100.0f);
      mvp       = glms_translate(mvp, (vec3s) { { 0.0f, 0.0f, 0.0f } }); /* not set as of now */

      ivec4s viewport = { 0 };
      glGetIntegerv(GL_VIEWPORT, viewport.raw);

      /**!
       * warn: let's not complicate things thinking about multiple fonts and
       * different line heights, single heights single font is fine for now.
       * let's make that work first.
       */
      f32 lineHeight = (f32) font->hbAscent - (f32) font->hbDescent;
      lineHeight *= (f32) fontScale;

      u32 visibleLineCount = (u32) windowHeight / (u32) lineHeight;
      if (lineCount < visibleLineCount)
         visibleLineCount = lineCount;

      glClearColor(ColorRGBAHex(0X002b36FF));

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      for (u32 lineIdx = 0; lineIdx < visibleLineCount; ++lineIdx)
      {
         i32 lineId                        = lineRenderers[lineIdx];
         struct LineRenderer *lineRenderer = editor->lineRenderer[lineId];

         lineRenderer->uniforms = (struct LineShaderUniforms) {
            .matViewProjection = mvp,
            .viewport          = viewport,
            .scale             = fontScale,
            .position          = { .x = 0, .y = ((f32) windowHeight - ((f32) lineHeight * ((f32) lineIdx + 1))) },
            .hbGpuAtlas        = atlas->textureUnit,
            .gamma             = 1.0f,
            .debug             = false,
            .stemDarkening     = false,
            .foreground        = (vec4s) { { ColorRGBAHex(0X839496FF) } },
         };

         lineShaderUploadUniforms(editor->lineShader, &lineRenderer->uniforms);

         if (lineRenderer->uploaded)
         {
            glBindVertexArray(lineRenderer->vao);
            glDrawArrays(GL_TRIANGLES, 0, (i32) lineRenderer->count);
         }
      }

      glfwSwapBuffers(window);
   }

   free(lineRenderers);
   fontManagerDeInit();

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

   /**!
    * note: Till we have a shared hidden window which
    * is destroyed at the end, we need to do this last
    */
   for (u32 idx = 0; idx < editor->windowCount; ++idx)
      glfwDestroyWindow(editor->window[idx]);

   lineShaderDeInit(editor->lineShader);

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
