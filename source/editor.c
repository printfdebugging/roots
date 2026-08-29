#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"

#include "editor.h"

int main(int argc, char *argv[])
{
   (void) argc;
   (void) argv;
   /**************************************************************************
    * NOTE: the code as of today breaks on scrolling when we have characters *
    * which use more than one bytes... it's intentional, the goal is to get  *
    * the rendering/scrolling working and then we fix these issues...        *
    * see  window.c keyPress                                                 *
    *************************************************************************/

   struct Editor *editor;

   if (!(editor = calloc(1, sizeof(struct Editor))) ||
       !(editor->lineLayout = calloc(1, sizeof(struct LineLayout))) ||
       !(editor->lineRenderer = calloc(1, sizeof(struct LineRenderer))))
   {
      perror("failed to allocate structs\n");
   }

   struct LineLayout *layout         = editor->lineLayout;
   struct LineRenderer *lineRenderer = editor->lineRenderer;

   windowInit(editor);
   editorInit(editor);
   lineLayoutInit(layout);
   lineRendererInit(lineRenderer);
   fontManagerInit(editor->fontFilePath);

   struct LineGlyphInfo lineGlyphInfo = { 0 };

   fontManagerMakeLineGlyphInfoSpec(&lineGlyphInfo, (char *) editor->lineBytes, editor->lineBytelen);
   lineLayoutGlyphQuadsFromInfo(layout, &lineGlyphInfo);
   lineRendererUploadLayoutQuadsToGPU(lineRenderer, layout);

   lineRendererCreateShader(lineRenderer);
   lineRendererSetupAttribLocations(lineRenderer);
   lineRendererCacheUniformLoc(lineRenderer);

   /*****************
    * the main loop *
    ****************/

   while (!glfwWindowShouldClose(editor->window))
   {
      /*********************
       * frame bookkeeping *
       ********************/

      f64 timeNow       = glfwGetTime();
      editor->timeDelta = timeNow - editor->lastTime;
      editor->lastTime  = timeNow;

      /*****************
       * event polling *
       ****************/

      glfwPollEvents();
      if (glfwGetKey(editor->window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
         glfwSetWindowShouldClose(editor->window, GLFW_TRUE);

      /******************************************
       * calculate the horizontal scroll offset *
       *****************************************/
      struct Font *font = fontManagerGetDefaultFont();
      u32 runeIdx       = layout->glyphQuadVertices[editor->cursorCol * 6].runeIdx;
      u32 cursorLeftPx  = (u32) (layout->glyphQuadVertices[editor->cursorCol * 6].x * lineRenderer->scale);
      u32 cursorWidthPx = (u32) (font->glyphCache[runeIdx].extents.xMax * lineRenderer->scale);
      u32 cursorRightPx = cursorLeftPx + cursorWidthPx;

      if (editor->xScrollOffset + (u32) editor->windowWidth < cursorRightPx)
         editor->xScrollOffset = cursorRightPx - (u32) editor->windowWidth;
      if (cursorLeftPx < editor->xScrollOffset)
         editor->xScrollOffset = cursorLeftPx;

      /***********************************
       * calculate transformation matrix *
       **********************************/
      lineRenderer->matViewProjection = glms_ortho(0, (f32) editor->windowWidth, 0, (f32) editor->windowHeight, 0.0f, 100.0f);
      lineRenderer->matViewProjection = glms_translate(lineRenderer->matViewProjection, (vec3s) { { -((f32) editor->xScrollOffset), 0.0f, 0.0f } });

      /********************
       * update variables *
       *******************/

      glGetIntegerv(GL_VIEWPORT, lineRenderer->viewport.raw);

      i32 xScale, yScale;
      hb_font_get_scale(font->hbFont, &xScale, &yScale);
      lineRenderer->scale = editor->fontSize / (f32) yScale;

      struct GlyphAtlas *atlas = fontManagerGetGlyphAtlas();
      lineRenderer->position.y = ((f32) editor->windowHeight - editor->fontSize) / 2;
      lineRenderer->gamma      = 1.0f;
      lineRenderer->debug      = false;
      lineRenderer->hbGpuAtlas = atlas->textureUnit;
      lineRenderer->runeIdx    = editor->cursorCol;

      /****************
       * set uniforms *
       ***************/

      glBindVertexArray(lineRenderer->glyphQuadVerticesVAO);

      lineRendererUploadUniforms(lineRenderer);

      /**********************
       * opengl: draw calls *
       *********************/

      glClearColor(ColorRGBAHex(0X282C33FF));
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      lineRendererRenderLine(lineRenderer);

      glfwSwapBuffers(editor->window);
   }

   /***********
    * cleanup *
    **********/

   lineRendererDeInit(lineRenderer);
   editorDeInit(editor);
   lineLayoutDeInit(layout);
   fontManagerDeInit();
   windowDeInit(editor);

   free(layout);
   free(lineRenderer);
   free(editor);
   free(lineGlyphInfo.glyphInfo);

   return EXIT_SUCCESS;
}

void editorInit(struct Editor *editor)
{
   u8 lineUTF8[]            = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
   const char *fontFilePath = ASSETS_DIR "LilexNerdFont-Regular.ttf";

   if (!(editor->lineBytelen = strlen((char *) lineUTF8)) ||
       !(editor->lineBytes = (byte *) stringDuplicate((char *) lineUTF8)))
   {
      perror("failed to decode lineUTF8\n");
   }

   editor->xScrollOffset = 0;
   editor->cursorCol     = 0;
   editor->cursorOffset  = 0;

   editor->fontSize     = 48.0f;
   editor->fontFilePath = stringDuplicate(fontFilePath);
}

void editorDeInit(struct Editor *editor)
{
   free(editor->fontFilePath);
   free(editor->lineBytes);
}
