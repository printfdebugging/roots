#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"
#include "hb-ot.h"

#include "editor.h"

struct Editor *editor;

int main(int argc, char *argv[])
{
   if (!(editor = calloc(1, sizeof(struct Editor))) ||
       !(editor->layout = calloc(1, sizeof(struct Layout))) ||
       !(editor->renderer = calloc(1, sizeof(struct Renderer))))
   {
      perror("failed to allocate structs\n");
   }

   struct Layout *layout     = editor->layout;
   struct Renderer *renderer = editor->renderer;

   editorInit(editor);
   layoutInit(layout);
   rendererInit(renderer);
   windowInit(editor);

   /* we have a window to draw stuff on */

   /* we have a stream of unicode codepoints to draw */

   /*********************************************************
    * harfbuzz: font loading & shape encoder initialization *
    ********************************************************/

   hb_blob_t *hbBlob = NULL;
   if (!(hbBlob = hb_blob_create_from_file(editor->fontFilePath)) ||
       !(layout->hbFace = hb_face_create(hbBlob, 0)) ||
       !(layout->hbFont = hb_font_create(layout->hbFace)) ||
       !(layout->hbDraw = hb_gpu_draw_create_or_fail()))
   {
      perror("failed to initialize harfbuzz");
   }

   hb_font_set_ptem(layout->hbFont, (f32) editor->displayDPI);
   hb_font_set_scale(layout->hbFont, (i32) editor->displayDPI * 1, (i32) editor->displayDPI * 1);
   hb_blob_destroy(hbBlob);

   /************************************************************
    * opengl: create an atlas texture to upload the glyph data *
    ***********************************************************/

   renderer->atlasCapacityBytes     = ATLAS_PAGE_SIZE;
   renderer->atlasCursorOffsetBytes = 0;
   glGenBuffers(1, &renderer->atlasTextureBufferObject);
   glBindBuffer(GL_TEXTURE_BUFFER, renderer->atlasTextureBufferObject);
   glBufferData(GL_TEXTURE_BUFFER, renderer->atlasCapacityBytes, NULL, GL_STATIC_DRAW);

   glActiveTexture(renderer->atlasTextureUnit);
   glGenTextures(1, &renderer->atlasTexture);
   glBindTexture(GL_TEXTURE_BUFFER, renderer->atlasTexture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, renderer->atlasTextureBufferObject);

   /******************************
    * glyph cache initialization *
    *****************************/

   layout->glyphCache = calloc(U16_MAX, sizeof(struct GlyphInfo));

   /*************************************
    * harfbuzz: get glyph ascent/decent *
    ************************************/

   const hb_ot_metrics_tag_t ASCENT_HHEA  = HB_TAG('H', 'a', 's', 'c');
   const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

   hb_position_t hbAscent, hbDescent, hbMaxHeight;
   hb_ot_metrics_get_position(layout->hbFont, ASCENT_HHEA, &hbAscent);
   hb_ot_metrics_get_position(layout->hbFont, DESCENT_HHEA, &hbDescent);
   hb_ot_metrics_get_position(layout->hbFont, HB_OT_METRICS_TAG_CAP_HEIGHT, &hbMaxHeight);

   /********************************************************
    * harfbuzz: shape the glyphs and get the glyph indices *
    *******************************************************/

   hb_buffer_t *buffer = hb_buffer_create();
   hb_buffer_add_codepoints(buffer, editor->lineRunes, editor->lineRunelen, 0, -1);
   hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
   hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
   hb_shape(layout->hbFont, buffer, NULL, 0);

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

      if (!layout->glyphCache[glyphIndex].cached)
      {
         i32 xScale, yScale;
         hb_font_get_scale(layout->hbFont, &xScale, &yScale);
         hb_gpu_draw_clear(layout->hbDraw);
         hb_gpu_draw_glyph(layout->hbDraw, layout->hbFont, glyphIndex);

         hb_glyph_extents_t hbGlyphExtents = {};
         hb_blob_t *hbBlob                 = NULL;

         hbBlob           = hb_gpu_draw_encode(layout->hbDraw, &hbGlyphExtents);
         u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

         /*****************************
          * cache the glyph quad info *
          ****************************/

         layout->glyphCache[glyphIndex] = (struct GlyphInfo) {
            .extents.xMin = 0,
            .extents.xMax = hb_font_get_glyph_h_advance(layout->hbFont, glyphIndex),
            .extents.yMin = hbDescent,
            .extents.yMax = hbAscent,
            .advance      = hb_font_get_glyph_h_advance(layout->hbFont, glyphIndex),
            .upem         = yScale,
            .empty        = (hbBlobLength == 0),
            .cached       = true,
         };

         /*********************************************************
          * upload glyph primitives to the gpu & store the offset *
          ********************************************************/

         if (!layout->glyphCache[glyphIndex].empty)
         {
            const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
            glBindBuffer(GL_TEXTURE_BUFFER, renderer->atlasTextureBufferObject);
            glBufferSubData(GL_TEXTURE_BUFFER, renderer->atlasCursorOffsetBytes, hbBlobLength, hbGlyphData);

            layout->glyphCache[glyphIndex].atlasOffset = renderer->atlasCursorOffsetBytes;
            renderer->atlasCursorOffsetBytes += hbBlobLength;

            hb_gpu_draw_recycle_blob(layout->hbDraw, hbBlob);
         }
      }

      /**************************************
       * load `glyphInfo` from `glyphCache` *
       *************************************/

      glyphInfo = layout->glyphCache[glyphIndex];

      /**********************
       * create glyph quads *
       *********************/
      /* todo: later create with ascent/decent rather than just advances */

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

   /************************************
    * opengl: glyph quad upload to gpu *
    ***********************************/
   glGenVertexArrays(1, &renderer->glyphQuadVerticesVAO);
   glGenBuffers(1, &renderer->glyphQuadVerticesVBO);

   glBindVertexArray(renderer->glyphQuadVerticesVAO);
   glBindBuffer(GL_ARRAY_BUFFER, renderer->glyphQuadVerticesVBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * layout->glyphQuadVerticesCount, layout->glyphQuadVertices, GL_STATIC_DRAW);
   renderer->glyphQuadsUploaded = true;

   /******************************************************************
    * opengl: create a shader `hbShaderProgram` for rendering glyphs *
    *****************************************************************/

   char *hbShaderVersion  = "#version 330 core\n";
   char *hbShaderPreamble = "#define HB_GPU_DEMO_DRAW\n";
   char *hbVertexMain     = readFileContents(ASSETS_DIR "harfbuzz.vert");
   char *hbFragmentMain   = readFileContents(ASSETS_DIR "harfbuzz.frag");

   u32 hbVertexShader;
   u32 hbFragmentShader;

   const char *hbVertexShaderSources[] = {
      hbShaderVersion,
      hbShaderPreamble,
      hb_gpu_shader_source(HB_GPU_SHADER_STAGE_VERTEX, HB_GPU_SHADER_LANG_GLSL),
      hb_gpu_draw_shader_source(HB_GPU_SHADER_STAGE_VERTEX, HB_GPU_SHADER_LANG_GLSL),
      hbVertexMain,
   };

   const char *hbFragmentShaderSources[] = {
      hbShaderVersion,
      hbShaderPreamble,
      hb_gpu_shader_source(HB_GPU_SHADER_STAGE_FRAGMENT, HB_GPU_SHADER_LANG_GLSL),
      hb_gpu_draw_shader_source(HB_GPU_SHADER_STAGE_FRAGMENT, HB_GPU_SHADER_LANG_GLSL),
      hbFragmentMain,
   };

   hbVertexShader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(hbVertexShader, ArraySize(hbVertexShaderSources), hbVertexShaderSources, NULL);
   glCompileShader(hbVertexShader);
   if (!shaderGetCompileStatus(hbVertexShader))
      perror("vertex shader compilation failed");

   hbFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(hbFragmentShader, ArraySize(hbFragmentShaderSources), hbFragmentShaderSources, NULL);
   glCompileShader(hbFragmentShader);
   if (!shaderGetCompileStatus(hbFragmentShader))
      perror("fragment shader compilation failed");

   renderer->hbShaderProgram = glCreateProgram();
   glAttachShader(renderer->hbShaderProgram, hbVertexShader);
   glAttachShader(renderer->hbShaderProgram, hbFragmentShader);
   glLinkProgram(renderer->hbShaderProgram);
   if (!shaderGetLinkStatus(renderer->hbShaderProgram))
      perror("failed to link shader program");

   glDeleteShader(hbVertexShader);
   glDeleteShader(hbFragmentShader);
   free(hbVertexMain);
   free(hbFragmentMain);

   /**********************************************************
    * opengl: setup attribute locations in `hbShaderProgram` *
    *********************************************************/

   i32 glyphQuadObjectStride = sizeof(struct GlyphVertex);
   i32 attribLocation        = -1;

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_position");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, x));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_texcoord");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, tx));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_normal");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, nx));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_emPerPos");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 1, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, emPerPos));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_glyphLoc");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, atlasOffset));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_runeIdx");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, runeIdx));

   glUseProgram(renderer->hbShaderProgram);

   /******************************************
    * opengl: cache shader uniform locations *
    *****************************************/

   renderer->hbShaderProgram_UniformLocation_matViewProjection = glGetUniformLocation(renderer->hbShaderProgram, "u_matViewProjection");
   renderer->hbShaderProgram_UniformLocation_viewport          = glGetUniformLocation(renderer->hbShaderProgram, "u_viewport");
   renderer->hbShaderProgram_UniformLocation_scale             = glGetUniformLocation(renderer->hbShaderProgram, "u_scale");
   renderer->hbShaderProgram_UniformLocation_position          = glGetUniformLocation(renderer->hbShaderProgram, "u_position");
   renderer->hbShaderProgram_UniformLocation_gamma             = glGetUniformLocation(renderer->hbShaderProgram, "u_gamma");
   renderer->hbShaderProgram_UniformLocation_foreground        = glGetUniformLocation(renderer->hbShaderProgram, "u_foreground");
   renderer->hbShaderProgram_UniformLocation_debug             = glGetUniformLocation(renderer->hbShaderProgram, "u_debug");
   renderer->hbShaderProgram_UniformLocation_stem_darkening    = glGetUniformLocation(renderer->hbShaderProgram, "u_stem_darkening");
   renderer->hbShaderProgram_UniformLocation_hb_gpu_atlas      = glGetUniformLocation(renderer->hbShaderProgram, "hb_gpu_atlas");
   renderer->hbShaderProgram_UniformLocation_runeIdx           = glGetUniformLocation(renderer->hbShaderProgram, "u_runeIdx");

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

      /**********************
       * update window size *
       *********************/
      i32 windowWidth, windowHeight;
      glfwGetWindowSize(editor->window, &windowWidth, &windowHeight);

      /******************************************
       * calculate the horizontal scroll offset *
       *****************************************/
      u32 runeIdx       = layout->glyphQuadVertices[editor->cursorCol * 6].runeIdx;
      u32 cursorLeftPx  = layout->glyphQuadVertices[editor->cursorCol * 6].x * renderer->hbUniform_scale;
      u32 cursorWidthPx = layout->glyphCache[runeIdx].extents.xMax * renderer->hbUniform_scale;
      u32 cursorRightPx = cursorLeftPx + cursorWidthPx;

      if (editor->xScrollOffset + windowWidth < cursorRightPx)
         editor->xScrollOffset = cursorRightPx - windowWidth;
      if (cursorLeftPx < editor->xScrollOffset)
         editor->xScrollOffset = cursorLeftPx;

      /***********************************
       * calculate transformation matrix *
       **********************************/
      renderer->hbUniform_matViewProjection = glms_ortho(0, windowWidth, 0, windowHeight, 0.0f, 100.0f);
      renderer->hbUniform_matViewProjection = glms_translate(renderer->hbUniform_matViewProjection, (vec3s) { -editor->xScrollOffset, 0.0f, 0.0f });

      /********************
       * update variables *
       *******************/

      glGetIntegerv(GL_VIEWPORT, renderer->hbUniform_viewport.raw);

      i32 xScale, yScale;
      hb_font_get_scale(layout->hbFont, &xScale, &yScale);
      renderer->hbUniform_scale = editor->fontSize / (f32) yScale;

      renderer->hbUniform_position.y   = (windowHeight - editor->fontSize) / 2;
      renderer->hbUniform_gamma        = 1.0f;
      renderer->hbUniform_debug        = false;
      renderer->hbUniform_hb_gpu_atlas = renderer->atlasTextureUnit;
      renderer->hbUniform_runeIdx      = editor->cursorCol;

      /****************
       * set uniforms *
       ***************/

      glBindVertexArray(renderer->glyphQuadVerticesVAO);

      glUniformMatrix4fv(renderer->hbShaderProgram_UniformLocation_matViewProjection, 1, GL_FALSE, renderer->hbUniform_matViewProjection.col[0].raw);
      glUniform4fv(renderer->hbShaderProgram_UniformLocation_foreground, 1, renderer->hbUniform_foreground.raw);
      glUniform2fv(renderer->hbShaderProgram_UniformLocation_position, 1, renderer->hbUniform_position.raw);
      glUniform2f(renderer->hbShaderProgram_UniformLocation_viewport, (f32) renderer->hbUniform_viewport.raw[2], (f32) renderer->hbUniform_viewport.raw[3]);
      glUniform1f(renderer->hbShaderProgram_UniformLocation_scale, (f32) renderer->hbUniform_scale);
      glUniform1f(renderer->hbShaderProgram_UniformLocation_stem_darkening, renderer->hbUniform_stem_darkening);
      glUniform1f(renderer->hbShaderProgram_UniformLocation_runeIdx, renderer->hbUniform_runeIdx);
      glUniform1f(renderer->hbShaderProgram_UniformLocation_debug, renderer->hbUniform_debug);
      glUniform1f(renderer->hbShaderProgram_UniformLocation_gamma, renderer->hbUniform_gamma);
      glUniform1i(renderer->hbShaderProgram_UniformLocation_hb_gpu_atlas, (i32) renderer->hbUniform_hb_gpu_atlas);

      /**********************
       * opengl: draw calls *
       *********************/

      glClearColor(ColorRGBAHex(0X282C34FF));
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glDrawArrays(GL_TRIANGLES, 0, (i32) layout->glyphQuadVerticesCount);

      glfwSwapBuffers(editor->window);
   }

   /***********
    * cleanup *
    **********/

   rendererDeInit(renderer);
   editorDeInit(editor);
   layoutDeInit(layout);
   windowDeInit(editor);

   free(layout);
   free(renderer);
   free(editor);

   return EXIT_SUCCESS;
}

void editorInit(struct Editor *editor)
{
   u8 lineUTF8[]       = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
   char *fontFilePath  = "/usr/share/fonts/TTF/IosevkaNerdFont-Regular.ttf";
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

   editor->fontSize     = 28.0f;
   editor->displayDPI   = 163.0f;
   editor->fontFilePath = calloc(fontFilePathLen + 1, sizeof(char));
   strcpy(editor->fontFilePath, fontFilePath);
}

void editorDeInit(struct Editor *editor)
{
   free(editor->lineRunes);
   free(editor->fontFilePath);
}
