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

   struct Editor *editor             = NULL;
   struct LineLayout *layout         = NULL;
   struct LineRenderer *lineRenderer = NULL;

   if (!(editor = calloc(1, sizeof(struct Editor))) ||
       !(layout = calloc(1, sizeof(struct LineLayout))) ||
       !(lineRenderer = calloc(1, sizeof(struct LineRenderer))))
   {
      perror("failed to allocate structs\n");
   }

   editorInit(editor);

   u8 lineUTF8[]     = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
   struct Text *text = textLoadFromData((char *) lineUTF8, (u32) strlen((char *) lineUTF8));
   editor->text      = text;
   byte *lineBytes   = (byte *) textGetUTF8Line(text, 0);
   u64 lineBytelen   = strlen((char *) lineBytes);

   /**!
    * note: we need to initialize windowing before anything gpu
    * related because that is what loads the glad pointers.
    */
   GLFWwindow *window = windowCreate();
   windowSetUserDataPtr(window, editor);

   lineLayoutInit(layout);
   lineRendererInit(lineRenderer);
   fontManagerInit(editor->fontFilePath);

   struct LineGlyphInfo lineGlyphInfo = {
      .cursorLine   = textGetCursorLine(text),
      .cursorColumn = textGetCursorColumn(text),
   };

   /* todo: layouting should layout and also take care of the cursor line.. so line should be marked with it */
   /* with multiple glyphs getting cursor flag, we can add multiple cursros to gui easily */
   fontManagerMakeLineGlyphInfoSpec(&lineGlyphInfo, (char *) lineBytes, lineBytelen);
   /* layouting is where we add cursor etc information, to be sent to the gpu.. */
   lineLayoutGlyphQuadsFromInfo(layout, &lineGlyphInfo);
   lineRendererUploadLayoutQuadsToGPU(lineRenderer, layout);

   lineRendererCreateShader(lineRenderer);
   lineRendererSetupAttribLocations(lineRenderer);
   lineRendererCacheUniformLoc(lineRenderer);

   /*****************
    * the main loop *
    ****************/

   /**!
    * note: properly define a data flow pipeline, it exists
    * but as of now is very loosely defined. this doesn't mean
    * create fancy abstractions, just keep in check what happens when..
    */
   while (!glfwWindowShouldClose(window))
   {
      /*********************
       * frame bookkeeping *
       ********************/

      f64 timeNow       = glfwGetTime();
      editor->timeDelta = timeNow - editor->lastTime;
      editor->lastTime  = timeNow;

      i32 windowWidth, windowHeight;
      glfwGetWindowSize(window, &windowWidth, &windowHeight);

      /*****************
       * event polling *
       ****************/

      glfwPollEvents();
      if (glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
         glfwSetWindowShouldClose(window, GLFW_TRUE);

      /* note: keep this a property of line. probably some way to map
       * text -> LineLayout.., buffer would have it all
       */
      if (editor->lineDirty)
      {
         /**!
          * note: a single key presses goes through
          * this quite a few times, that shouldn't happen.
          * if the cursor is not moving, no reason to mark
          * the line dirty for now (later we might have treesitter
          * etc updating the lines, so that would be different.)
          * fprintf(stderr, "dirty\n");
          */
         lineGlyphInfo = (struct LineGlyphInfo) {
            .cursorLine   = textGetCursorLine(text),
            .cursorColumn = textGetCursorColumn(text),
         };

         fontManagerMakeLineGlyphInfoSpec(&lineGlyphInfo, (char *) lineBytes, lineBytelen);
         lineLayoutGlyphQuadsFromInfo(layout, &lineGlyphInfo);
         lineRendererUploadLayoutQuadsToGPU(lineRenderer, layout);
      }

      /******************************************
       * calculate the horizontal scroll offset *
       *****************************************/
      struct Font *font = fontManagerGetDefaultFont();
      u32 cursorColumn  = textGetCursorColumn(text);
      u32 cursorLeftPx  = (u32) (layout->glyphQuadVertices[cursorColumn * 6].x * lineRenderer->rendererOpts.scale);
      u32 cursorWidthPx = (u32) (font->glyphCache[cursorColumn].extents.xMax * lineRenderer->rendererOpts.scale);
      u32 cursorRightPx = cursorLeftPx + cursorWidthPx;

      if (editor->xScrollOffset + (u32) windowWidth < cursorRightPx)
         editor->xScrollOffset = cursorRightPx - (u32) windowWidth;
      if (cursorLeftPx < editor->xScrollOffset)
         editor->xScrollOffset = cursorLeftPx;

      /***********************************
       * calculate transformation matrix *
       **********************************/
      lineRenderer->rendererOpts.matViewProjection = glms_ortho(0, (f32) windowWidth, 0, (f32) windowHeight, 0.0f, 100.0f);
      lineRenderer->rendererOpts.matViewProjection = glms_translate(lineRenderer->rendererOpts.matViewProjection, (vec3s) { { -((f32) editor->xScrollOffset), 0.0f, 0.0f } });

      /********************
       * update variables *
       *******************/

      glGetIntegerv(GL_VIEWPORT, lineRenderer->rendererOpts.viewport.raw);

      i32 xScale, yScale;
      hb_font_get_scale(font->hbFont, &xScale, &yScale);
      lineRenderer->rendererOpts.scale = editor->fontSize / (f32) yScale;

      struct GlyphAtlas *atlas              = fontManagerGetGlyphAtlas();
      lineRenderer->rendererOpts.position.y = ((f32) windowHeight - editor->fontSize) / 2;
      lineRenderer->rendererOpts.gamma      = 1.0f;
      lineRenderer->rendererOpts.debug      = false;
      lineRenderer->rendererOpts.hbGpuAtlas = atlas->textureUnit;

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

      glfwSwapBuffers(window);
   }

   /***********
    * cleanup *
    **********/

   lineRendererDeInit(lineRenderer);
   editorDeInit(editor);
   lineLayoutDeInit(layout);
   fontManagerDeInit();
   windowDestroy(window);

   free(layout);
   free(lineRenderer);
   free(editor);
   free(lineGlyphInfo.glyphInfo);

   return EXIT_SUCCESS;
}

void editorInit(struct Editor *editor)
{
   const char *fontFilePath = ASSETS_DIR "LilexNerdFont-Regular.ttf";
   editor->xScrollOffset    = 0;
   editor->lineDirty        = false;

   editor->fontSize     = 48.0f;
   editor->fontFilePath = stringDuplicate(fontFilePath);
}

void editorDeInit(struct Editor *editor)
{
   free(editor->fontFilePath);
}
