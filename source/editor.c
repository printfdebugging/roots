#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"

#include "editor.h"

struct Editor *editor;

void magicFunction(struct Font *font, struct FontLayout *layout, struct FontRenderer *fontRenderer);

int main(int argc, char *argv[])
{
   if (!(editor = calloc(1, sizeof(struct Editor))) ||
       !(editor->fontLayout = calloc(1, sizeof(struct FontLayout))) ||
       !(editor->fontRenderer = calloc(1, sizeof(struct FontRenderer))))
   {
      perror("failed to allocate structs\n");
   }

   struct FontLayout *layout         = editor->fontLayout;
   struct FontRenderer *fontRenderer = editor->fontRenderer;

   windowInit(editor);
   editorInit(editor);
   fontLayoutInit(layout, editor->fontFilePath);
   fontRendererInit(fontRenderer);
   fontManagerInit();

   struct Font *font = fontManagerGetFont(editor->fontFilePath);

   magicFunction(font, layout, fontRenderer);

   /**************************************************************************
    * layouting/relayouting, shared glue code between renderer and layouting *
    *************************************************************************/
   glBindVertexArray(fontRenderer->glyphQuadVerticesVAO);
   glBindBuffer(GL_ARRAY_BUFFER, fontRenderer->glyphQuadVerticesVBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * layout->glyphQuadVerticesCount, layout->glyphQuadVertices, GL_STATIC_DRAW);
   fontRenderer->glyphQuadsUploaded = true;

   fontRendererCreateShader(fontRenderer);
   fontRendererSetupAttribLocations(fontRenderer);
   fontRendererCacheUniformLoc(fontRenderer);

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
      u32 runeIdx       = layout->glyphQuadVertices[editor->cursorCol * 6].runeIdx;
      u32 cursorLeftPx  = layout->glyphQuadVertices[editor->cursorCol * 6].x * fontRenderer->scale;
      u32 cursorWidthPx = font->glyphCache[runeIdx].extents.xMax * fontRenderer->scale;
      u32 cursorRightPx = cursorLeftPx + cursorWidthPx;

      if (editor->xScrollOffset + editor->windowWidth < cursorRightPx)
         editor->xScrollOffset = cursorRightPx - editor->windowWidth;
      if (cursorLeftPx < editor->xScrollOffset)
         editor->xScrollOffset = cursorLeftPx;

      /***********************************
       * calculate transformation matrix *
       **********************************/
      fontRenderer->matViewProjection = glms_ortho(0, editor->windowWidth, 0, editor->windowHeight, 0.0f, 100.0f);
      fontRenderer->matViewProjection = glms_translate(fontRenderer->matViewProjection, (vec3s) { -editor->xScrollOffset, 0.0f, 0.0f });

      /********************
       * update variables *
       *******************/

      glGetIntegerv(GL_VIEWPORT, fontRenderer->viewport.raw);

      i32 xScale, yScale;
      hb_font_get_scale(font->hbFont, &xScale, &yScale);
      fontRenderer->scale = editor->fontSize / (f32) yScale;

      fontRenderer->position.y = (editor->windowHeight - editor->fontSize) / 2;
      fontRenderer->gamma      = 1.0f;
      fontRenderer->debug      = false;
      fontRenderer->hbGpuAtlas = fontRenderer->atlasTextureUnit;
      fontRenderer->runeIdx    = editor->cursorCol;

      /****************
       * set uniforms *
       ***************/

      glBindVertexArray(fontRenderer->glyphQuadVerticesVAO);

      fontRendererUploadUniforms(fontRenderer);

      /**********************
       * opengl: draw calls *
       *********************/

      glClearColor(ColorRGBAHex(0X282C33FF));
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      /* note: this info, the renderer should have already i think */
      glDrawArrays(GL_TRIANGLES, 0, (i32) layout->glyphQuadVerticesCount);
      glfwSwapBuffers(editor->window);
   }

   /***********
    * cleanup *
    **********/

   fontRendererDeInit(fontRenderer);
   editorDeInit(editor);
   fontLayoutDeInit(layout);
   fontManagerDeInit();
   windowDeInit(editor);

   free(layout);
   free(fontRenderer);
   free(editor);

   return EXIT_SUCCESS;
}

void editorInit(struct Editor *editor)
{
   u8 lineUTF8[]       = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
   char *fontFilePath  = ASSETS_DIR "LilexNerdFont-Regular.ttf";
   u32 fontFilePathLen = strlen(fontFilePath);

   if (!(editor->lineBytelen = strlen((char *) lineUTF8)) ||
       !(editor->lineRunelen = uc_rune_count(lineUTF8, editor->lineBytelen)) ||
       !(editor->lineRunes = calloc(editor->lineRunelen, sizeof(rune))) ||
       !(uc_utf8_decode_stream(lineUTF8, editor->lineBytelen, editor->lineRunes, editor->lineRunelen)))
   {
      perror("failed to decode lineUTF8\n");
   }

   editor->xScrollOffset = 0;
   editor->cursorCol     = 0;
   editor->cursorOffset  = 0;

   editor->fontSize     = 48.0f;
   editor->fontFilePath = calloc(fontFilePathLen + 1, sizeof(char));
   strcpy(editor->fontFilePath, fontFilePath);
}

void editorDeInit(struct Editor *editor)
{
   free(editor->lineRunes);
   free(editor->fontFilePath);
}

void magicFunction(struct Font *font, struct FontLayout *layout, struct FontRenderer *fontRenderer)
{
   /********************************************************
    * harfbuzz: shape the glyphs and get the glyph indices *
    *******************************************************/

   hb_buffer_t *buffer = hb_buffer_create();
   hb_buffer_add_codepoints(buffer, editor->lineRunes, editor->lineRunelen, 0, -1);
   hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
   hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
   hb_shape(font->hbFont, buffer, NULL, 0);

   u32 glyphCount                      = 0;
   hb_glyph_info_t *glyphInfos         = hb_buffer_get_glyph_infos(buffer, &glyphCount);
   hb_glyph_position_t *glyphPositions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

   /*************************************************
    * harfbuzz: allocate space to store glyph quads *
    ************************************************/

   layout->glyphQuadVerticesCount = glyphCount * 6;
   layout->glyphQuadVertices      = calloc(layout->glyphQuadVerticesCount, sizeof(struct GlyphVertex));

   /******************************************
    * harfbuzz: load & cache font glyph data *
    *****************************************/

   struct Point glyphPosition = { .x = 0, .y = 0 };
   for (u32 glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx)
   {
      hb_codepoint_t glyphIndex  = glyphInfos[glyphIdx].codepoint;
      struct GlyphInfo glyphInfo = { 0 };

      /**************************************************************
       * harfbuzz: cache the glyph primitives if not cached already *
       *************************************************************/

      /* still font specific */
      if (!font->glyphCache[glyphIndex].cached)
      {
         i32 xScale, yScale;
         hb_font_get_scale(font->hbFont, &xScale, &yScale);
         hb_gpu_draw_clear(font->hbDraw);
         hb_gpu_draw_glyph(font->hbDraw, font->hbFont, glyphIndex);

         hb_glyph_extents_t hbGlyphExtents = {};
         hb_blob_t *hbBlob                 = NULL;

         hbBlob           = hb_gpu_draw_encode(font->hbDraw, &hbGlyphExtents);
         u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

         /*****************************
          * cache the glyph quad info *
          ****************************/

         font->glyphCache[glyphIndex] = (struct GlyphInfo) {
            .extents.xMin = 0,
            .extents.xMax = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
            .extents.yMin = font->hbDescent,
            .extents.yMax = font->hbAscent,
            .advance      = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
            .upem         = yScale,
            .empty        = (hbBlobLength == 0),
            .cached       = true,
         };

         /*********************************************************
          * upload glyph primitives to the gpu & store the offset *
          ********************************************************/

         /* still font caching */
         if (!font->glyphCache[glyphIndex].empty)
         {
            const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
            glBindBuffer(GL_TEXTURE_BUFFER, fontRenderer->atlasTextureBufferObject);
            glBufferSubData(GL_TEXTURE_BUFFER, fontRenderer->atlasCursorOffsetBytes, hbBlobLength, hbGlyphData);

            font->glyphCache[glyphIndex].atlasOffset = fontRenderer->atlasCursorOffsetBytes;
            fontRenderer->atlasCursorOffsetBytes += hbBlobLength;

            hb_gpu_draw_recycle_blob(font->hbDraw, hbBlob);
         }
      }

      /**************************************
       * load `glyphInfo` from `glyphCache` *
       *************************************/

      glyphInfo = font->glyphCache[glyphIndex];

      /**********************
       * create glyph quads *
       *********************/

      glyphPosition.x += glyphPositions[glyphIdx].x_offset;
      glyphPosition.y += glyphPositions[glyphIdx].y_offset;

      struct GlyphVertex glyphQuadCorners[4];
      for (int cornerIdx = 0; cornerIdx < 4; cornerIdx++)
      {
         i32 cx = (cornerIdx >> 1) & 1;
         i32 cy = cornerIdx & 1;
         f64 ex = (1 - cx) * glyphInfo.extents.xMin + cx * glyphInfo.extents.xMax;
         f64 ey = (1 - cy) * glyphInfo.extents.yMin + cy * glyphInfo.extents.yMax;

         glyphQuadCorners[cornerIdx] = (struct GlyphVertex) {
            .x           = (f32) glyphPosition.x,
            .y           = (f32) glyphPosition.y,
            .tx          = (f32) ex,
            .ty          = (f32) ey,
            .nx          = cx ? 1.f : -1.f,
            .ny          = cy ? -1.f : 1.f,
            .emPerPos    = 1.0,
            .atlasOffset = glyphInfo.atlasOffset / TEXEL_SIZE,
            .runeIdx     = glyphIdx,
         };
      }

      u32 glyphQuadOffset = glyphIdx * 6;

      layout->glyphQuadVertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
      layout->glyphQuadVertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
      layout->glyphQuadVertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
      layout->glyphQuadVertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
      layout->glyphQuadVertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
      layout->glyphQuadVertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

      glyphPosition.x += glyphPositions[glyphIdx].x_advance;
      glyphPosition.y += glyphPositions[glyphIdx].y_advance;
   }

   hb_buffer_destroy(buffer);
}
