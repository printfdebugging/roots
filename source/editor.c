#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"

#include "editor.h"

bool editorRun(struct Editor *editor)
{
   editorInit(editor);

   GLFWwindow *window = windowCreate((struct GLFWwindowOptions) {
      .width       = 800,
      .height      = 600,
      .title       = "GLFWwindow",
      .transparent = true,
      .visible     = true,
      .fbResizeFn  = windowResize,
      .keyFn       = keyPress,
      .userdata    = editor,
   });

   fontManagerInit(editor->fontFilePath);

   /**!
    * `LineShader` is shared between all the renderers. It is mostly stateless
    * and only stores the uniform locations, not the values themselves. The
    * values live in `LineShaderUniforms` struct and the `lineShaderUploadUniforms`
    * function uploads the uniforms before drawing a particular line.
    */
   struct LineShader *lineShader = calloc(1, sizeof(struct LineShader));
   lineShaderInit(lineShader);

   i32 textId = editorLoadTextFile(editor, ASSETS_DIR "test.md");

   struct Text *text = editor->text[textId];
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
   }

   /* I think here we can use struct of arrays rather than array of structs.. */
   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      lineRendererDeInit(&lineRenderer[lineIdx]);
   }

   free(lineRenderer);
   lineShaderDeInit(lineShader);
   free(lineShader);

   fontManagerDeInit();
   windowDestroy(window);

   return true;
}

bool editorInit(struct Editor *editor)
{
   if (editor->initialized)
      return true;

   editor->fontSize = DEFAULT_FONT_SIZE;
   if (!(editor->fontFilePath = stringDuplicate(DEFAULT_FONT_FILE_PATH)))
      return false;

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
   free(editor->fontFilePath);
   for (u32 idx = 0; idx < editor->textCount; ++idx)
   {
      textDestroy(editor->text[idx]);
      free(editor->text[idx]);
   }

   free(editor->text);

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

   u32 count    = editor->textCount++;
   editor->text = realloc(editor->text, sizeof(struct Text *) * (count + 1));

   if (!editor->text)
      return -1;

   editor->text[count] = text;
   return (i32) count;
}
