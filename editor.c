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

#include "./types.h"
#include "./macros.h"

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

/********************
 * Helper functions *
 *******************/

static char *readFileContents(const char *filPath);
static b8 shaderGetCompileStatus(u32 shaderObject);
static b8 shaderGetLinkStatus(u32 shaderProgram);

/**************************
 * line rendering globals *
 *************************/

u32 lineVao     = 0;
u32 lineVbo     = 0;
u32 lineProgram = 0;
u32 lineBytelen = 0;
u32 lineRunelen = 0;
u8 lineUTF8[]   = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
rune *lineRunes = NULL;

i32 cursorCol = 0;

/********************
 * harfbuzz globals *
 *******************/

f32 fontSize       = 28.0f;
f32 displayDPI     = 163.0f;
char *fontFilePath = "/usr/share/fonts/TTF/IosevkaNerdFont-Regular.ttf";

hb_face_t *hbFace     = NULL;
hb_font_t *hbFont     = NULL;
hb_gpu_draw_t *hbDraw = NULL;
u32 hbShaderProgram   = 0;

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

u32 atlasTexture             = 0;
u32 atlasTextureUnit         = GL_TEXTURE0;
u32 atlasTextureBufferObject = 0;
u32 atlasCapacityBytes       = 0;
u32 atlasCursorOffsetBytes   = 0;

struct GlyphInfo *glyphCache = NULL;

struct GlyphVertex *glyphQuadVertices = NULL;
u32 glyphQuadVerticesCount            = 0;
u32 glyphQuadVerticesVAO              = 0;
u32 glyphQuadVerticesVBO              = 0;
b32 glyphQuadsUploaded                = false;

int main(int argc, char *argv[])
{
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

   /* we have a window to draw stuff on */

   /*****************************************
    * line rendering globals initialization *
    ****************************************/

   if (!(lineBytelen = strlen((char *) lineUTF8)) ||
       !(lineRunelen = uc_rune_count(lineUTF8, lineBytelen)) ||
       !(lineRunes = calloc(lineRunelen, sizeof(rune))) ||
       !(uc_utf8_decode_stream(lineUTF8, lineBytelen, lineRunes, lineRunelen)))
   {
      perror("failed to decode lineUTF8\n");
   }

   /* we have a stream of unicode codepoints to draw */

   /*********************************************************
    * harfbuzz: font loading & shape encoder initialization *
    ********************************************************/

   hb_blob_t *hbBlob = NULL;
   if (!(hbBlob = hb_blob_create_from_file(fontFilePath)) ||
       !(hbFace = hb_face_create(hbBlob, 0)) ||
       !(hbFont = hb_font_create(hbFace)) ||
       !(hbDraw = hb_gpu_draw_create_or_fail()))
   {
      perror("failed to initialize harfbuzz");
   }

   hb_font_set_ptem(hbFont, (f32) displayDPI);
   hb_font_set_scale(hbFont, (i32) displayDPI * 1, (i32) displayDPI * 1);
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

   glyphCache = calloc(U16_MAX, sizeof(struct GlyphInfo));

   /*************************************
    * harfbuzz: get glyph ascent/decent *
    ************************************/

   const hb_ot_metrics_tag_t ASCENT_HHEA  = HB_TAG('H', 'a', 's', 'c');
   const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

   hb_position_t hbAscent, hbDescent, hbMaxHeight;
   hb_ot_metrics_get_position(hbFont, ASCENT_HHEA, &hbAscent);
   hb_ot_metrics_get_position(hbFont, DESCENT_HHEA, &hbDescent);
   hb_ot_metrics_get_position(hbFont, HB_OT_METRICS_TAG_CAP_HEIGHT, &hbMaxHeight);

   /********************************************************
    * harfbuzz: shape the glyphs and get the glyph indices *
    *******************************************************/

   hb_buffer_t *buffer = hb_buffer_create();
   hb_buffer_add_codepoints(buffer, lineRunes, lineRunelen, 0, -1);
   hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
   hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
   hb_shape(hbFont, buffer, NULL, 0);

   u32 glyphCount                      = 0;
   hb_glyph_info_t *glyphInfos         = hb_buffer_get_glyph_infos(buffer, &glyphCount);
   hb_glyph_position_t *glyphPositions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

   /*************************************************
    * harfbuzz: allocate space to store glyph quads *
    ************************************************/

   glyphQuadVerticesCount = glyphCount * 6;
   glyphQuadVertices      = calloc(glyphQuadVerticesCount, sizeof(struct GlyphVertex));

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

      if (!glyphCache[glyphIndex].cached)
      {
         i32 xScale, yScale;
         hb_font_get_scale(hbFont, &xScale, &yScale);
         hb_gpu_draw_clear(hbDraw);
         hb_gpu_draw_glyph(hbDraw, hbFont, glyphIndex);

         hb_glyph_extents_t hbGlyphExtents = {};
         hb_blob_t *hbBlob                 = NULL;

         hbBlob           = hb_gpu_draw_encode(hbDraw, &hbGlyphExtents);
         u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

         /*****************************
          * cache the glyph quad info *
          ****************************/

         glyphCache[glyphIndex] = (struct GlyphInfo) {
            .extents.min_x = 0,
            .extents.max_x = hb_font_get_glyph_h_advance(hbFont, glyphIndex),
            .extents.min_y = hbDescent,
            .extents.max_y = hbAscent,
            .advance       = hb_font_get_glyph_h_advance(hbFont, glyphIndex),
            .upem          = yScale,
            .empty         = (hbBlobLength == 0),
            .cached        = true,
         };

         /*********************************************************
          * upload glyph primitives to the gpu & store the offset *
          ********************************************************/

         if (!glyphCache[glyphIndex].empty)
         {
            const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
            glBindBuffer(GL_TEXTURE_BUFFER, atlasTextureBufferObject);
            glBufferSubData(GL_TEXTURE_BUFFER, atlasCursorOffsetBytes, hbBlobLength, hbGlyphData);

            glyphCache[glyphIndex].atlas_offset = atlasCursorOffsetBytes;
            atlasCursorOffsetBytes += hbBlobLength;

            hb_gpu_draw_recycle_blob(hbDraw, hbBlob);
         }
      }

      /**************************************
       * load `glyphInfo` from `glyphCache` *
       *************************************/

      glyphInfo = glyphCache[glyphIndex];

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
         f64 ex = (1 - cx) * glyphInfo.extents.min_x + cx * glyphInfo.extents.max_x;
         f64 ey = (1 - cy) * glyphInfo.extents.min_y + cy * glyphInfo.extents.max_y;

         glyphQuadCorners[cornerIdx] = (struct GlyphVertex) {
            .x            = (f32) glyphPosition.x,
            .y            = (f32) glyphPosition.y,
            .tx           = (f32) ex,
            .ty           = (f32) ey,
            .nx           = cx ? 1.f : -1.f,
            .ny           = cy ? -1.f : 1.f,
            .emPerPos     = 1.0,
            .atlas_offset = glyphInfo.atlas_offset / TEXEL_SIZE,
            .runeIdx      = glyphIdx,
         };
      }

      u32 glyphQuadOffset = glyphIdx * 6;

      glyphQuadVertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
      glyphQuadVertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
      glyphQuadVertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
      glyphQuadVertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
      glyphQuadVertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
      glyphQuadVertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

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
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * glyphQuadVerticesCount, glyphQuadVertices, GL_STATIC_DRAW);
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
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, atlas_offset));

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

      /***********************************
       * calculate transformation matrix *
       **********************************/

      i32 windowWidth, windowHeight;
      glfwGetWindowSize(window, &windowWidth, &windowHeight);
      hbUniform_matViewProjection = glms_ortho(0, windowWidth, 0, windowHeight, 0.0f, 100.0f);

      /********************
       * update variables *
       *******************/

      glGetIntegerv(GL_VIEWPORT, hbUniform_viewport);

      i32 xScale, yScale;
      hb_font_get_scale(hbFont, &xScale, &yScale);
      hbUniform_scale = fontSize / (f32) yScale;

      hbUniform_position.y   = (windowHeight - fontSize) / 2;
      hbUniform_gamma        = 1.0f;
      hbUniform_debug        = false;
      hbUniform_hb_gpu_atlas = atlasTextureUnit;
      hbUniform_runeIdx      = cursorCol;

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

      glDrawArrays(GL_TRIANGLES, 0, (i32) glyphQuadVerticesCount);

      glfwSwapBuffers(window);
   }

   /***********
    * cleanup *
    **********/

   hb_face_destroy(hbFace);
   hb_font_destroy(hbFont);
   hb_gpu_draw_destroy(hbDraw);

   free(lineRunes);
   free(glyphQuadVertices);

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

char *readFileContents(const char *filPath)
{
   char *data = NULL;
   FILE *file = fopen(filPath, "rb");
   if (!file)
   {
      fprintf(stderr, "failed to read shader file: %s\n", filPath);
      return NULL;
   }

   fseek(file, 0, SEEK_END);
   i64 length = ftell(file);
   fseek(file, 0, SEEK_SET);

   if (length < 0)
   {
      fprintf(stderr, "failed to get the shader file's length: %s\n", filPath);
      goto CLEANUP;
   }

   if (!(data = calloc(1, length + 1)))
   {
      fprintf(stderr, "failed to allocate memory for data to store file %s\n", filPath);
      goto CLEANUP;
   }

   u64 read_count = fread(data, 1, length, file);
   if (read_count < length || read_count == 0)
   {
      fprintf(stderr, "read returned %li which is either 0 or less than %li", read_count, length);
      goto CLEANUP;
   }

   data[length] = '\0';
   if (fclose(file))
   {
      fprintf(stderr, "fclose failed\n");
      goto CLEANUP;
   }

   return data;

CLEANUP:
   fclose(file);
   free(data);
   return NULL;
}

b8 shaderGetCompileStatus(u32 shaderObject)
{
   i32 compileStatus;
   glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
   if (compileStatus)
      return true;

   i32 logLength;
   glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &logLength);

   char infoLog[logLength];
   glGetShaderInfoLog(shaderObject, logLength, NULL, infoLog);
   fprintf(stderr, "failed to compile shader, error message: %s\n", infoLog);
   return false;
}

b8 shaderGetLinkStatus(u32 shaderProgram)
{
   i32 linkStatus;
   glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
   if (linkStatus)
      return true;

   i32 logLength;
   glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

   char infoLog[logLength];
   glGetProgramInfoLog(shaderProgram, logLength, NULL, infoLog);
   fprintf(stderr, "failed to link shader program: %s\n", infoLog);
   return false;
}
