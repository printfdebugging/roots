#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"

#include "editor.h"

/**!
 * later:
 * we don't need to support unicode this early, we are using utf8
 * data structures, but from that to rendering unicode perfectly is
 * a really long journey and we can do a lot of things before we nail
 * that down and we should go this route..
 */
int main(int argc, char *argv[])
{
   (void) argc;
   (void) argv;

   struct Editor *editor = calloc(1, sizeof(struct Editor));
   editorInit(editor);

   /**!
    * warning: fixme:
    * We need to initialize windowing before anything GPU
    * related because that is what loads the glad pointers.
    *
    * - FontManager
    * - Shaders
    */
   GLFWwindow *window = windowCreate();
   windowSetUserDataPtr(window, editor);
   u32 xScrollOffset = 0;

   fontManagerInit(editor->fontFilePath);

   /**!
    * `LineShader` is shared between all the renderers. It is mostly stateless
    * and only stores the uniform locations, not the values themselves. The
    * values live in `LineShaderUniforms` struct and the `lineShaderUploadUniforms`
    * function uploads the uniforms before drawing a particular line.
    */
   struct LineShader *lineShader = calloc(1, sizeof(struct LineShader));
   lineShaderInit(lineShader);

   struct Text *text = textLoadFromFile(ASSETS_DIR "test.md");
   editor->text      = text;
   u32 lineCount     = textGetLineCount(text);

   struct LineRenderer *lineRenderer = calloc(lineCount, sizeof(struct LineRenderer));

   if (!lineRenderer)
      perror("failed to allocate memory\n");

   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      lineRendererInit(&lineRenderer[lineIdx], lineShader);

      char *lineBytes = textGetUTF8Line(text, lineIdx);
      u64 lineByteLen = strlen(lineBytes);

      fontManagerLayoutLine(&lineRenderer[lineIdx], (char *) lineBytes, lineByteLen);
   }

   /**!
    * note:
    * properly define a data flow pipeline, it exists
    * but as of now is very loosely defined. this doesn't mean
    * create fancy abstractions, just keep in check what happens when..
    */
   while (!glfwWindowShouldClose(window))
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
      mvp       = glms_translate(mvp, (vec3s) { { -((f32) xScrollOffset), 0.0f, 0.0f } }); /* not set as of now */

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
         lineRenderer[lineIdx].uniforms = (struct LineShaderUniforms) {
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

         struct LineRenderer *renderer = &lineRenderer[lineIdx];
         lineShaderUploadUniforms(lineShader, &renderer->uniforms);

         if (renderer->uploaded)
         {
            glBindVertexArray(renderer->vao);
            glDrawArrays(GL_TRIANGLES, 0, (i32) renderer->count);
         }
      }

      glfwSwapBuffers(window);

      /**!
       * later:
       * let's ignore horizontal scrolling for now. we can always take care of it later on..
       * same for vertical scrolling.. incremntal steps, let's show the lines first
       */
      // struct Font *font = fontManagerGetDefaultFont();
      // u32 cursorColumn  = textGetCursorColumn(text);
      // u32 cursorLeftPx  = (u32) (layout->glyphQuadVertices[cursorColumn * 6].x * lineRenderer->uniforms.scale);
      // u32 cursorWidthPx = (u32) (font->glyphCache[cursorColumn].extents.xMax * lineRenderer->uniforms.scale);
      // u32 cursorRightPx = cursorLeftPx + cursorWidthPx;
      //
      // if (xScrollOffset + (u32) windowWidth < cursorRightPx)
      //    xScrollOffset = cursorRightPx - (u32) windowWidth;
      // if (cursorLeftPx < xScrollOffset)
      //    xScrollOffset = cursorLeftPx;
   }

   /* I think here we can use struct of arrays rather than array of structs.. */
   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      lineRendererDeInit(&lineRenderer[lineIdx]);
   }

   free(lineRenderer);
   lineShaderDeInit(lineShader);
   free(lineShader);

   editorDeInit(editor);
   fontManagerDeInit();

   windowDestroy(window);
   free(editor);

   return EXIT_SUCCESS;
}

void editorInit(struct Editor *editor)
{
   const char *fontFilePath = ASSETS_DIR "LilexNerdFont-Regular.ttf";
   editor->lineDirty        = false;

   editor->fontSize     = 24.0f;
   editor->fontFilePath = stringDuplicate(fontFilePath);
}

void editorCalcFrameTime(struct Editor *editor)
{
   f64 timeNow       = glfwGetTime();
   editor->timeDelta = timeNow - editor->lastTime;
   editor->lastTime  = timeNow;
}

void editorDeInit(struct Editor *editor)
{
   free(editor->fontFilePath);
   textDestroy(editor->text);
   free(editor->text);
}
