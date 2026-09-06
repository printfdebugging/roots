#include <string.h>

#include "glad/glad.h"

#include "editor.h"

/* warn: todo: add cleanup at some later stage when it works */
/* returns a buffer id.. todo: write nicely later, let's first make it work */
i32 editorOpenFile(struct Editor *editor, const char *path)
{
   struct Buffer *buf = NULL;
   i32 textId;

   if (!editor->initialized)
      goto failure;
   if ((textId = editorLoadTextFile(editor, path)) == -1)
      goto failure;
   if (!(buf = calloc(1, sizeof(struct Buffer))))
      goto failure;

   *buf = (struct Buffer) {
      .winId            = -1,
      .txtId            = textId,
      .editor           = NULL,
      .cursorColumn     = 0,
      .cursorLine       = 0,
      .hOffset          = 0,
      .vOffset          = 0,
      .visLineRenderers = NULL,
      .visLineCount     = 0,
   };

   GLFWwindow *sharedWin = NULL;
   if (editor->sharedWindowId != -1)
      sharedWin = editor->window[editor->sharedWindowId];

   buf->winId = editorCreateWindow(
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
          .shared      = sharedWin,
       }
   );

   /* todo: move these to editorInit */
   fontManagerInit(editor->fontFilePath);
   lineShaderInit(editor->lineShader);

   struct Text *text = editor->text[textId];
   u32 lineCount     = textGetLineCount(text);

   /* todo: allocate these just for the visible lines, not for all the lines. */
   if (!(buf->visLineRenderers = calloc(lineCount, sizeof(i32))))
      goto failure;

   memset(buf->visLineRenderers, -1, sizeof(i32) * lineCount);

   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      /* todo: a line should hold a text id and a line number
       * just to be more aware of where it is coming from.. */
      buf->visLineRenderers[lineIdx] = editorCreateLine(
          editor,
          (struct LineOptions) {
             .lineIdx = lineIdx,
             .textId  = textId,
          }
      );
   }

   if (!(editor->textBuffer = realloc(editor->textBuffer, sizeof(struct Buffer *) * (editor->bufferCount + 1))))
      goto failure;

   editor->textBuffer[editor->bufferCount] = buf;
   return (i32) editor->bufferCount++;

failure:
   if (buf)
      free(buf->visLineRenderers);
   free(buf);

   return -1;
}

void editorDrawBuffer(struct Editor *editor, i32 bufId)
{
   /* todo: fix this with new API over IDs */
   GLFWwindow *window = editor->window[editor->textBuffer[bufId]->winId];
   /* todo: move to render function */
   i32 windowWidth, windowHeight;
   glfwGetWindowSize(window, &windowWidth, &windowHeight);

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

   struct Buffer *buf = editor->textBuffer[bufId];

   glClearColor(ColorRGBAHex(0X002b36FF));

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   struct Text *text = editor->text[buf->txtId];
   u32 lineCount     = textGetLineCount(text);

   buf->visLineCount = (u32) windowHeight / (u32) lineHeight;
   if (lineCount < buf->visLineCount)
      buf->visLineCount = lineCount;

   for (u32 lineIdx = 0; lineIdx < buf->visLineCount; ++lineIdx)
   {
      i32 lineId = buf->visLineRenderers[lineIdx];

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
