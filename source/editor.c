#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include "cglm/struct.h"
#include "unicode/unicode.h"
#include "hb.h"
#include "hb-gpu.h"
#include "hb-ot.h"

#include "editor.h"

/******************
 * window globals *
 *****************/

GLFWwindow *window = NULL;
f32 lastTime;
f32 timeDelta;

/********************
 * window callbacks *
 *******************/

static void mouseScroll(GLFWwindow *window, f64 x, f64 y);
static void windowResize(GLFWwindow *window, i32 width, i32 height);
static void mouseMove(GLFWwindow *window, f64 x, f64 y);
static void keyPress(GLFWwindow *window, int key, int scancode, int action, int mods);

struct Editor *editor;

/*************************************
 * renderer - draw uniform locations *
 ************************************/
u32 hbShaderProgram;
i32 hbShaderProgram_UniformLocation_matViewProjection = -1;
i32 hbShaderProgram_UniformLocation_viewport          = -1;
f32 hbShaderProgram_UniformLocation_scale             = -1;
i32 hbShaderProgram_UniformLocation_position          = -1;
i32 hbShaderProgram_UniformLocation_hb_gpu_atlas      = -1;
i32 hbShaderProgram_UniformLocation_gamma             = -1;
i32 hbShaderProgram_UniformLocation_foreground        = -1;
i32 hbShaderProgram_UniformLocation_debug             = -1;
i32 hbShaderProgram_UniformLocation_stem_darkening    = -1;
i32 hbShaderProgram_UniformLocation_runeIdx           = -1;

/**********************************
 * renderer - draw uniform states *
 *********************************/
mat4s hbUniform_matViewProjection = { GLM_MAT4_IDENTITY_INIT };
i32 hbUniform_viewport[4]         = { 0 };
f32 hbUniform_scale               = 0;
vec2s hbUniform_position          = { 0 };
i32 hbUniform_hb_gpu_atlas        = 0;
f32 hbUniform_gamma               = 0;
vec4s hbUniform_foreground        = { ColorRGBAHex(0XD8DEE9FF) };
b8 hbUniform_debug                = false;
b8 hbUniform_stem_darkening       = false;
i32 hbUniform_runeIdx             = 0;

/*********************************
 * renderer - object store/cache *
 ********************************/
u32 atlasTexture             = 0;
u32 atlasTextureUnit         = GL_TEXTURE0;
u32 atlasTextureBufferObject = 0;
u32 atlasCapacityBytes       = 0;
u32 atlasCursorOffsetBytes   = 0;

/**************************
 * renderer - layout data *
 *************************/
u32 glyphQuadVerticesVAO = 0;
u32 glyphQuadVerticesVBO = 0;
b32 glyphQuadsUploaded   = false;

int main(int argc, char *argv[])
{
   if (!(editor = calloc(1, sizeof(struct Editor))) ||
       !(editor->layout = calloc(1, sizeof(struct Layout))))
   {
      perror("failed to allocate structs\n");
   }

   struct Layout *layout = editor->layout;

   editorInit(editor);
   layoutInit(layout);

   /*************************
    * window initialization *
    ************************/

   glfwInit();
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
   glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
   glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
   glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
   glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

   window = glfwCreateWindow(1600, 200, "GLFWWindow", NULL, NULL);
   glfwMakeContextCurrent(window);
   gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
   glfwSwapInterval(1);

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_MULTISAMPLE);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glLineWidth(2);

   glfwSetCursorPosCallback(window, mouseMove);
   glfwSetScrollCallback(window, mouseScroll);
   glfwSetFramebufferSizeCallback(window, windowResize);
   glfwSetKeyCallback(window, keyPress);

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

   atlasCapacityBytes     = ATLAS_PAGE_SIZE;
   atlasCursorOffsetBytes = 0;
   glGenBuffers(1, &atlasTextureBufferObject);
   glBindBuffer(GL_TEXTURE_BUFFER, atlasTextureBufferObject);
   glBufferData(GL_TEXTURE_BUFFER, atlasCapacityBytes, NULL, GL_STATIC_DRAW);

   glActiveTexture(atlasTextureUnit);
   glGenTextures(1, &atlasTexture);
   glBindTexture(GL_TEXTURE_BUFFER, atlasTexture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, atlasTextureBufferObject);

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
            glBindBuffer(GL_TEXTURE_BUFFER, atlasTextureBufferObject);
            glBufferSubData(GL_TEXTURE_BUFFER, atlasCursorOffsetBytes, hbBlobLength, hbGlyphData);

            layout->glyphCache[glyphIndex].atlasOffset = atlasCursorOffsetBytes;
            atlasCursorOffsetBytes += hbBlobLength;

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
   glGenVertexArrays(1, &glyphQuadVerticesVAO);
   glGenBuffers(1, &glyphQuadVerticesVBO);

   glBindVertexArray(glyphQuadVerticesVAO);
   glBindBuffer(GL_ARRAY_BUFFER, glyphQuadVerticesVBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * layout->glyphQuadVerticesCount, layout->glyphQuadVertices, GL_STATIC_DRAW);
   glyphQuadsUploaded = true;

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

   hbShaderProgram = glCreateProgram();
   glAttachShader(hbShaderProgram, hbVertexShader);
   glAttachShader(hbShaderProgram, hbFragmentShader);
   glLinkProgram(hbShaderProgram);
   if (!shaderGetLinkStatus(hbShaderProgram))
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

   attribLocation = glGetAttribLocation(hbShaderProgram, "a_position");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, x));

   attribLocation = glGetAttribLocation(hbShaderProgram, "a_texcoord");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, tx));

   attribLocation = glGetAttribLocation(hbShaderProgram, "a_normal");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, nx));

   attribLocation = glGetAttribLocation(hbShaderProgram, "a_emPerPos");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 1, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, emPerPos));

   attribLocation = glGetAttribLocation(hbShaderProgram, "a_glyphLoc");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, atlasOffset));

   attribLocation = glGetAttribLocation(hbShaderProgram, "a_runeIdx");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, runeIdx));

   glUseProgram(hbShaderProgram);

   /******************************************
    * opengl: cache shader uniform locations *
    *****************************************/

   hbShaderProgram_UniformLocation_matViewProjection = glGetUniformLocation(hbShaderProgram, "u_matViewProjection");
   hbShaderProgram_UniformLocation_viewport          = glGetUniformLocation(hbShaderProgram, "u_viewport");
   hbShaderProgram_UniformLocation_scale             = glGetUniformLocation(hbShaderProgram, "u_scale");
   hbShaderProgram_UniformLocation_position          = glGetUniformLocation(hbShaderProgram, "u_position");
   hbShaderProgram_UniformLocation_gamma             = glGetUniformLocation(hbShaderProgram, "u_gamma");
   hbShaderProgram_UniformLocation_foreground        = glGetUniformLocation(hbShaderProgram, "u_foreground");
   hbShaderProgram_UniformLocation_debug             = glGetUniformLocation(hbShaderProgram, "u_debug");
   hbShaderProgram_UniformLocation_stem_darkening    = glGetUniformLocation(hbShaderProgram, "u_stem_darkening");
   hbShaderProgram_UniformLocation_hb_gpu_atlas      = glGetUniformLocation(hbShaderProgram, "hb_gpu_atlas");
   hbShaderProgram_UniformLocation_runeIdx           = glGetUniformLocation(hbShaderProgram, "u_runeIdx");

   /*****************
    * the main loop *
    ****************/

   while (!glfwWindowShouldClose(window))
   {
      /*********************
       * frame bookkeeping *
       ********************/

      f64 timeNow = glfwGetTime();
      timeDelta   = timeNow - lastTime;
      lastTime    = timeNow;

      /*****************
       * event polling *
       ****************/

      glfwPollEvents();
      if (glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
         glfwSetWindowShouldClose(window, GLFW_TRUE);

      /**********************
       * update window size *
       *********************/
      i32 windowWidth, windowHeight;
      glfwGetWindowSize(window, &windowWidth, &windowHeight);

      /******************************************
       * calculate the horizontal scroll offset *
       *****************************************/
      u32 runeIdx       = layout->glyphQuadVertices[editor->cursorCol * 6].runeIdx;
      u32 cursorLeftPx  = layout->glyphQuadVertices[editor->cursorCol * 6].x * hbUniform_scale;
      u32 cursorWidthPx = layout->glyphCache[runeIdx].extents.xMax * hbUniform_scale;
      u32 cursorRightPx = cursorLeftPx + cursorWidthPx;

      if (editor->xScrollOffset + windowWidth < cursorRightPx)
         editor->xScrollOffset = cursorRightPx - windowWidth;
      if (cursorLeftPx < editor->xScrollOffset)
         editor->xScrollOffset = cursorLeftPx;

      /***********************************
       * calculate transformation matrix *
       **********************************/
      hbUniform_matViewProjection = glms_ortho(0, windowWidth, 0, windowHeight, 0.0f, 100.0f);
      hbUniform_matViewProjection = glms_translate(hbUniform_matViewProjection, (vec3s) { -editor->xScrollOffset, 0.0f, 0.0f });

      /********************
       * update variables *
       *******************/

      glGetIntegerv(GL_VIEWPORT, hbUniform_viewport);

      i32 xScale, yScale;
      hb_font_get_scale(layout->hbFont, &xScale, &yScale);
      hbUniform_scale = editor->fontSize / (f32) yScale;

      hbUniform_position.y   = (windowHeight - editor->fontSize) / 2;
      hbUniform_gamma        = 1.0f;
      hbUniform_debug        = false;
      hbUniform_hb_gpu_atlas = atlasTextureUnit;
      hbUniform_runeIdx      = editor->cursorCol;

      /****************
       * set uniforms *
       ***************/

      glBindVertexArray(glyphQuadVerticesVAO);

      glUniformMatrix4fv(hbShaderProgram_UniformLocation_matViewProjection, 1, GL_FALSE, hbUniform_matViewProjection.col[0].raw);
      glUniform4fv(hbShaderProgram_UniformLocation_foreground, 1, hbUniform_foreground.raw);
      glUniform2fv(hbShaderProgram_UniformLocation_position, 1, hbUniform_position.raw);
      glUniform2f(hbShaderProgram_UniformLocation_viewport, (f32) hbUniform_viewport[2], (f32) hbUniform_viewport[3]);
      glUniform1f(hbShaderProgram_UniformLocation_scale, (f32) hbUniform_scale);
      glUniform1f(hbShaderProgram_UniformLocation_stem_darkening, hbUniform_stem_darkening);
      glUniform1f(hbShaderProgram_UniformLocation_runeIdx, hbUniform_runeIdx);
      glUniform1f(hbShaderProgram_UniformLocation_debug, hbUniform_debug);
      glUniform1f(hbShaderProgram_UniformLocation_gamma, hbUniform_gamma);
      glUniform1i(hbShaderProgram_UniformLocation_hb_gpu_atlas, (i32) hbUniform_hb_gpu_atlas);

      /**********************
       * opengl: draw calls *
       *********************/

      glClearColor(ColorRGBAHex(0X282C34FF));
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glDrawArrays(GL_TRIANGLES, 0, (i32) layout->glyphQuadVerticesCount);

      glfwSwapBuffers(window);
   }

   /***********
    * cleanup *
    **********/

   editorDeInit(editor);
   layoutDeInit(layout);

   free(editor);
   free(layout);

   glfwDestroyWindow(window);
   glfwTerminate();

   return EXIT_SUCCESS;
}

void windowResize(GLFWwindow *window, i32 width, i32 height)
{
   glViewport(0, 0, width, height);
}

void mouseScroll(GLFWwindow *window, f64 x, f64 y)
{
}

void mouseMove(GLFWwindow *window, f64 x, f64 y)
{
}

void keyPress(GLFWwindow *window, int key, int scancode, int action, int mods)
{
   /**************************
    * update cursor location *
    *************************/
   if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
   {
      editor->cursorCol -= 1;
      if (editor->cursorCol < 0)
         editor->cursorCol = 0;
   }

   if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
   {
      editor->cursorCol += 1;
      if (editor->cursorCol >= editor->lineRunelen)
         editor->cursorCol = editor->lineRunelen - 1;
   }
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
