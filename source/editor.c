#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"
#include "stb_image.h"
#include "hb-ot.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <dwmapi.h>
#include "GLFW/glfw3native.h"
#endif

#include "editor.h"

static struct Editor E = { 0 };

void _glfwErrFn(int code, const char *description);

bool run()
{
   const char *path = ASSETS_DIR "test.md";
   if (!E.initialized)
      perror("E not initialized\n");
   if (!loadTextFile(path))
      perror("Failed to load Text file\n");

   while (!shouldClose())
   {
      calcFrameTime();
      glfwPollEvents();

      layout();
      upload();
      render();
   }

   return true;
}

bool shouldClose()
{
   if (!E.initialized)
      return true;

   bool shouldClose = true;
   shouldClose &= glfwWindowShouldClose(E.window);

   return shouldClose;
}

bool init()
{
   if (E.initialized)
      return true;

   E.fontSize = DEFAULT_FONT_SIZE;
   if (!(E.fontFilePath = stringDuplicate(DEFAULT_FONT_FILE_PATH)))
      return false;

   /**!
    * note: Context sharing means that the objects will be shared i.e. they would be
    * available in the other context, so we won't have to reallocate/reupload them, but
    * we have to bind them manually to the container objects on the other context (VAOs)
    * in order to use them.
    */
   if (!createWindow((struct GLFWwindowOptions) {
          .width = 800,
          .height = 600,
          .title = "GLFWwindow",
          .transparent = false,
          .visible = true,
          .icon = DEFAULT_WINDOW_ICON,
          .sharedWinId = INVALID_ID,
          .fbResizeFn = fbResizeFn,
          .keyFn = keyFn,
          .scrollFn = scrollFn,
          .curPosFn = curPosFn,
       }))
   {
      perror("failed to create a window");
   }

   glfwSetErrorCallback(_glfwErrFn);

   if (!(E.lineShader = calloc(1, sizeof(struct TextShader))))
      return false;
   if (!(E.bufRenderer = calloc(1, sizeof(struct BufferRenderer))))
      return false;

   fmInit(E.fontFilePath);
   createTextShader(E.lineShader);
   initBufferRenderer(E.bufRenderer, E.lineShader);

   E.initialized = true;
   return true;
}

void calcFrameTime()
{
   f64 tNow = glfwGetTime();
   E.tDelta = tNow - E.tLast;
   E.tLast = tNow;
}

bool deInit()
{
   for (i32 idx = 0; idx < E.lineLayoutCount; ++idx)
      free(E.lineLayout[idx]->vertices);
   free(E.lineLayout);

   /* note: todo: maybe this should be above the window destruction sequence */
   destroyTextShader(E.lineShader);
   deInitBufferRenderer(E.bufRenderer);
   fmDeInit();

   textDestroy(E.text);
   free(E.text);

   free(E.lineLayout);
   free(E.lineShader);
   free(E.fontFilePath);

   /**!
    * note: Till we have a shared hidden window which
    * is destroyed at the end, we need to do this last
    */
   glfwDestroyWindow(E.window);
   glfwTerminate();

   return true;
}

void render()
{
   renderBuffer();
}

/* error: todo: upload has some issue for sure. qrenderdoc
 * says that all the vertex attribute data is 0, which is
 * the first thing we should chase here. */
void upload()
{
   if (E.lineLayoutCount == 0)
      return;

   u32 totalCount = 0;
   for (i32 idx = 0; idx < E.lineLayoutCount; ++idx)
      totalCount += E.lineLayout[idx]->count;

   /* upload to GPU */
   if (totalCount != E.bufRenderer->count)
   {
      u32 unitSize = sizeof(struct GlyphVertex);
      glBindVertexArray(E.bufRenderer->vao);
      glBindBuffer(GL_ARRAY_BUFFER, E.bufRenderer->vbo);
      glBufferData(GL_ARRAY_BUFFER, totalCount * unitSize, NULL, GL_STATIC_DRAW);

      u32 uploadedCount = 0;
      for (i32 idx = 0; idx < E.lineLayoutCount; ++idx)
      {
         struct LineLayout *layout = E.lineLayout[idx];
         glBufferSubData(GL_ARRAY_BUFFER, uploadedCount * unitSize, layout->count * unitSize, layout->vertices);
         uploadedCount += layout->count;
      }

      E.bufRenderer->count = uploadedCount;
      E.bufRenderer->uploaded = true;
   }
}

void layout()
{
   layoutBuffer();
}

/**!
 * Loads the text file from `filePath` into a `Text` object,
 * and returns an index to it, or `INVALID_ID` on error.
 */
bool loadTextFile(const char *filePath)
{
   if (!filePath)
      return false;
   if (!(E.text = textLoadFromFile(filePath)))
      return false;
   return true;
}

/* warn: todo: add cleanup at some later stage when it works */
/* returns a buffer id.. todo: write nicely later, let's first make it work */
void openFile(const char *path)
{
   (void) path;
   /* todo: */
}

/* todo: remove editor from here */
void renderBuffer()
{
   struct GlyphAtlas *atlas = fmGetAtlas();
   struct Rectangle bounds = getWindowBounds();

   mat4s mvp = { GLM_MAT4_IDENTITY_INIT };
   mvp = glms_ortho(0, (f32) bounds.w, 0, (f32) bounds.h, 0.0f, 100.0f);
   mvp = glms_translate(mvp, (vec3s) { { 0.0f, 0.0f, 0.0f } }); /* not set as of now */

   ivec4s viewport = { 0 };
   glGetIntegerv(GL_VIEWPORT, viewport.raw);

   glClearColor(ColorRGBAHex(0X002b36FF));
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   f32 fontScale = fmGetDefaultFontScale();

   E.bufRenderer->uniforms = (struct TextShaderUniforms) {
      .matViewProjection = mvp,
      .viewport = viewport,
      .scale = fontScale,
      .hbGpuAtlas = atlas->textureUnit,
      .gamma = 1.0f,
      .debug = false,
      .stemDarkening = false,
   };

   uploadTextShaderUniforms(E.lineShader, &E.bufRenderer->uniforms);

   if (E.bufRenderer->uploaded)
   {
      glBindVertexArray(E.bufRenderer->vao);
      glDrawArrays(GL_TRIANGLES, 0, (i32) E.bufRenderer->count);
   }

   /* note: not sure if this should be done after each buffer is rendered, or after all of them
    * are rendered. for now we just do it here since we only have a single buffer. */
   swapBuffers();
}

static bool doneOnce = false;

struct GlyphInfo *_glyphInfo = NULL;

void layoutBuffer()
{
   if (!E.lineShader)
      return;
   if (!E.text)
      return;

   /* note: this is so that we get to see something on the screen first.
    * once we have that, we can make this politically correct ;) */
   if (doneOnce)
      return;

   /* todo: some way to layout/show only the visible part & relayout on some event */
   /* todo: some mechanism to mark a line dirty here */
   /* todo: check the dirty line count and the editor */
   /* todo: relayout only when the quads change. for stuff like color changes, cursor movement, we can just send the diffs to the gpu to make it really quick */

   u32 lineCount = textGetLineCount(E.text);
   f32 lineHeight = fmGetDefaultFontLineHeight();
   f32 fontScale = fmGetDefaultFontScale();

   struct Rectangle bounds = getWindowBounds();

   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      struct LineLayout *layout = calloc(1, sizeof(struct LineLayout));
      if (!layout)
         return;

      /* todo: hide strlen behind the text api so that we can later replace it with something more efficient. */
      char *lineBytes = textGetUTF8Line(E.text, lineIdx);
      u64 lineByteLen = strlen(lineBytes);
      /* this should take layouting options.. */

      /* todo: next: question: why is it that when i send 10 the text doesn't move and when i send 10 / fontScale it does move? */
      struct LineLayoutOpts opts = {
         .lineUTF8 = lineBytes,
         .lineByteLen = lineByteLen,
         .position = { .x = 0, .y = ((f32) bounds.h - ((f32) (lineIdx + 1) * lineHeight)) / fontScale },
      };

      /* this already does the layouting :) */
      /*
       * todo: refactor:
       * - there's a lot more to line layouting than just position and text..
       * - at some point even this function would be gone and all we would have is "layoutBuffer + a loop over each line".
       * - this intution seems right because currently LineLayoutOpts is the pipeline to pass all that info to layouting,
       *   the foreground color etc.. per character essentially, and this struct just isn't sufficient for that.
       * - treesitter i think uses utf8 streams, and fmLayoutLine too does that.. so would be intresting to see how they fit together
       */
      if (!E.fm.initialized)
         return;

      struct Font *font = fmGetDefaultFont();

      hb_buffer_t *buffer = hb_buffer_create();
      hb_buffer_add_utf8(buffer, opts.lineUTF8, -1, 0, -1);
      hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
      hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
      hb_shape(font->hbFont, buffer, NULL, 0);

      u32 hbGlyphCount = 0;
      hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &hbGlyphCount);

      _glyphInfo = realloc(_glyphInfo, hbGlyphCount * sizeof(struct GlyphInfo));
      memset(_glyphInfo, 0, hbGlyphCount * sizeof(struct GlyphInfo));

      for (u32 glyphIdx = 0; glyphIdx < hbGlyphCount; ++glyphIdx)
      {
         hb_codepoint_t glyphIndex = glyphInfos[glyphIdx].codepoint;
         struct GlyphInfo *glyph = &font->glyphCache[glyphIndex];
         if (!glyph->cached)
         {
            i32 xScale, yScale;
            hb_font_get_scale(font->hbFont, &xScale, &yScale);
            hb_gpu_draw_clear(font->hbDraw);
            hb_gpu_draw_glyph(font->hbDraw, font->hbFont, glyphIndex);

            hb_glyph_extents_t hbGlyphExtents = {};
            hb_blob_t *hbBlob = NULL;

            hbBlob = hb_gpu_draw_encode(font->hbDraw, &hbGlyphExtents);
            u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

            *glyph = (struct GlyphInfo) {
               .extents.xMin = 0,
               .extents.xMax = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
               .extents.yMin = font->hbDescent,
               .extents.yMax = font->hbAscent,
               .advance = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
               .upem = yScale,
               .empty = (hbBlobLength == 0),
               .cached = true,
            };

            /* upload glyph data to glyph atlas */
            struct GlyphAtlas *glyphAtlas = fmGetAtlas();
            if (!glyph->empty)
            {
               const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
               glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->textureBufferObject);
               glBufferSubData(GL_TEXTURE_BUFFER, glyphAtlas->cursorOffsetBytes, hbBlobLength, hbGlyphData);
               glyph->atlasOffset = glyphAtlas->cursorOffsetBytes;
               glyphAtlas->cursorOffsetBytes += hbBlobLength;

               hb_gpu_draw_recycle_blob(font->hbDraw, hbBlob);
            }
         }

         _glyphInfo[glyphIdx] = *glyph;
      }

      hb_buffer_destroy(buffer);

      layout->count = hbGlyphCount * 6;
      layout->vertices = realloc(layout->vertices, layout->count * sizeof(struct GlyphVertex));

      struct Point glyphPosition = {
         .x = opts.position.x,
         .y = opts.position.y,
      };

      for (u32 glyphIdx = 0; glyphIdx < hbGlyphCount; ++glyphIdx)
      {
         [[maybe_unused]] bool hasCursor;
         struct GlyphInfo *glyphInfo = &_glyphInfo[glyphIdx];

         /**********************
          * create glyph quads *
          *********************/

         glyphPosition.x += glyphInfo->extents.xMin;
         glyphPosition.y += 0;

         struct GlyphVertex glyphQuadCorners[4];
         for (int cornerIdx = 0; cornerIdx < 4; cornerIdx++)
         {
            i32 cx = (cornerIdx >> 1) & 1;
            i32 cy = cornerIdx & 1;
            f64 ex = (1 - cx) * glyphInfo->extents.xMin + cx * glyphInfo->extents.xMax;
            f64 ey = (1 - cy) * glyphInfo->extents.yMin + cy * glyphInfo->extents.yMax;

            glyphQuadCorners[cornerIdx] = (struct GlyphVertex) {
               .x = (f32) glyphPosition.x,
               .y = (f32) glyphPosition.y,
               .tx = (f32) ex,
               .ty = (f32) ey,
               .nx = cx ? 1.f : -1.f,
               .ny = cy ? -1.f : 1.f,
               .emPerPos = 1.0,
               .atlasOffset = glyphInfo->atlasOffset / TEXEL_SIZE,
               .hasCursor = false,
               .fgColor = (vec4s) { { ColorRGBAHex(0X839496FF) } },
               .bgColor = (vec4s) { { ColorRGBAHex(0X000000FF) } },
               /* next: fix this. for now, nothing has a cursor */
            };
         }

         u32 glyphQuadOffset = glyphIdx * 6;

         layout->vertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
         layout->vertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
         layout->vertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
         layout->vertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
         layout->vertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
         layout->vertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

         /* note: this currently assumes the layout to be horizontal, fine assumption
          * when starting out, but later we would also want to cater for the vertical
          * writing styles. */
         glyphPosition.x += glyphInfo->extents.xMax;
      }

      if (!(E.lineLayout = realloc(E.lineLayout, sizeof(struct LineLayout *) * ((u32) E.lineLayoutCount + 1))))
      {
         free(layout);
         return;
      }

      E.lineLayout[E.lineLayoutCount++] = layout;
   }
   doneOnce = true;
}

bool createWindow(struct GLFWwindowOptions opts)
{
   if (!glfwInit())
      return false;

   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, opts.transparent);
   glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
   glfwWindowHint(GLFW_VISIBLE, opts.visible);
   glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
   glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

#ifdef DEBUG
   glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#endif

   const i32 windowWidth = opts.width ? opts.width : 1600;
   const i32 windowHeight = opts.height ? opts.height : 800;
   const char *windowTitle = opts.title ? opts.title : "GLFWwindow";

   /* todo: re-implement it later */
   GLFWwindow *sharedWindow = NULL;

   GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, sharedWindow);
   if (!window)
      return false;

   glfwMakeContextCurrent(window);
   gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
   glfwSwapInterval(1);

#ifndef __APPLE__
   GLFWimage img;
   int chanCount;
   opts.icon = opts.icon ? opts.icon : DEFAULT_WINDOW_ICON;
   img.pixels = stbi_load(opts.icon, &img.width, &img.height, &chanCount, 0);

   if (!img.pixels)
   {
      glfwDestroyWindow(window);
      return false;
   }

   glfwSetWindowIcon(window, 1, &img);
   free(img.pixels);
#endif

#ifdef _WIN32
   HWND hwnd = glfwGetWin32Window(window);
   DWORD value = _msIsDarkMode();
   DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
#endif

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_MULTISAMPLE);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glLineWidth(2);

   if (opts.curPosFn) glfwSetCursorPosCallback(window, opts.curPosFn);
   if (opts.scrollFn) glfwSetScrollCallback(window, opts.scrollFn);
   if (opts.fbResizeFn) glfwSetFramebufferSizeCallback(window, opts.fbResizeFn);
   if (opts.keyFn) glfwSetKeyCallback(window, opts.keyFn);

   if (!window)
      return false;

   E.window = window;
   return true;
}

struct Rectangle getWindowBounds()
{
   if (!E.window)
      return (struct Rectangle) {};

   i32 windowWidth, windowHeight;
   glfwGetWindowSize(E.window, &windowWidth, &windowHeight);

   return (struct Rectangle) {
      .x = 0,
      .y = 0,
      .w = windowWidth,
      .h = windowHeight,
   };
}

void swapBuffers()
{
   glfwSwapBuffers(E.window);
}

void _glfwErrFn(int code, const char *description)
{
   fprintf(stderr, "_glfwErrFun: code: %i, msg: %s\n", code, description);
}

#ifdef _WIN32
static bool _msIsDarkMode()
{
   HINSTANCE uxThemeLib = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
   if (!uxThemeLib)
   {
      fprintf(stderr, "failed to open uxtheme.dll\n");
      return true; /* default to dark mode */
   }

   bool useDarkMode = GetProcAddress(uxThemeLib, MAKEINTRESOURCEA(132))();
   FreeLibrary(uxThemeLib);
   return useDarkMode;
}
#endif

void fbResizeFn(GLFWwindow *window, i32 width, i32 height)
{
   (void) window;
   glViewport(0, 0, width, height);
}

void scrollFn(GLFWwindow *window, f64 x, f64 y)
{
   (void) window;
   (void) x;
   (void) y;
}

void curPosFn(GLFWwindow *window, f64 x, f64 y)
{
   (void) window;
   (void) x;
   (void) y;
}

void keyFn(GLFWwindow *window, int key, int scancode, int action, int mods)
{
   (void) scancode;
   [[maybe_unused]] struct Editor *editor = glfwGetWindowUserPointer(window);

   bool shiftQPress = (mods & GLFW_MOD_SHIFT) && (key == GLFW_KEY_Q) && (action == GLFW_PRESS);
   if (shiftQPress)
      glfwSetWindowShouldClose(window, GLFW_TRUE);
}

/**!
 * note: FontManager is just a wrapper around harfbuzz & OpenGL functions,
 * and manages shared objects.. So the OpenGL function pointers should be
 * loaded before this function is called. That's done by GLFW.
 */
void fmInit(char *editorFontPath)
{
   if (E.fm.initialized)
      return;

   /* `fontManagerGetFont` checks this and returns early if false (default) */
   E.fm.initialized = true;

   _fmAtlasInit();
   E.fm.editorFontPath = stringDuplicate(editorFontPath);
   E.fm.editorFont = fmGetFont(editorFontPath);
}

void fmDeInit()
{
   if (!E.fm.initialized)
      return;

   for (u32 fontIdx = 0; fontIdx < E.fm.fontCount; ++fontIdx)
      fntDeInit(&E.fm.font[fontIdx]);

   free(E.fm.font);
   free((void *) E.fm.editorFontPath);

   _fmAtlasDeInit();
   E.fm.initialized = false;
}

struct GlyphAtlas *fmGetAtlas()
{
   if (!E.fm.initialized)
      return NULL;
   return &E.fm.glyphAtlas;
}

struct Font *fmGetFont(const char *filePath)
{
   if (!E.fm.initialized)
      return NULL;

   for (u32 fontIdx = 0; fontIdx < E.fm.fontCount; ++fontIdx)
      if (strcmp(E.fm.font[fontIdx].fontPath, filePath) == 0)
         return &E.fm.font[fontIdx];

   E.fm.font = realloc(E.fm.font, sizeof(struct Font) * (E.fm.fontCount + 1));
   struct Font *font = &E.fm.font[E.fm.fontCount++];
   fntInit(font, filePath);

   return font;
}

struct Font *fmGetDefaultFont()
{
   if (!E.fm.initialized)
      return NULL;
   return E.fm.editorFont;
}

f32 fmGetDefaultFontScale()
{
   if (!E.fm.initialized)
      return 0;

   i32 xScale, yScale;
   hb_font_get_scale(E.fm.editorFont->hbFont, &xScale, &yScale);
   return E.fontSize / (f32) yScale;
}

f32 fmGetDefaultFontLineHeight()
{
   if (!E.fm.initialized)
      return 0;

   f32 lineHeight = (f32) E.fm.editorFont->hbAscent - (f32) E.fm.editorFont->hbDescent;
   return lineHeight * fmGetDefaultFontScale();
}

struct Font *fmGetFontWithRune(rune codepoint)
{
   (void) codepoint;
   perror("todo");
   return NULL;
}

void fntInit(struct Font *font, const char *filePath)
{
   font->fontPath = stringDuplicate(filePath);
   font->glyphCache = calloc(U16_MAX, sizeof(struct GlyphInfo));

   hb_blob_t *hbBlob = NULL;
   if (!(hbBlob = hb_blob_create_from_file(font->fontPath)) ||
       !(font->hbFace = hb_face_create(hbBlob, 0)) ||
       !(font->hbFont = hb_font_create(font->hbFace)) ||
       !(font->hbDraw = hb_gpu_draw_create_or_fail()))
   {
      perror("failed to initialize harfbuzz");
   }

   hb_blob_destroy(hbBlob);

   const hb_ot_metrics_tag_t ASCENT_HHEA = HB_TAG('H', 'a', 's', 'c');
   const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

   hb_ot_metrics_get_position(font->hbFont, ASCENT_HHEA, &font->hbAscent);
   hb_ot_metrics_get_position(font->hbFont, DESCENT_HHEA, &font->hbDescent);
   hb_ot_metrics_get_position(font->hbFont, HB_OT_METRICS_TAG_CAP_HEIGHT, &font->hbMaxHeight);
}

void fntDeInit(struct Font *font)
{
   hb_font_destroy(font->hbFont);
   hb_face_destroy(font->hbFace);
   hb_gpu_draw_destroy(font->hbDraw);

   free(font->fontPath);
   free(font->glyphCache);
}

void _fmAtlasInit()
{
   struct GlyphAtlas *glyphAtlas = &E.fm.glyphAtlas;

   glyphAtlas->capacityBytes = ATLAS_PAGE_SIZE;
   glyphAtlas->cursorOffsetBytes = TEXEL_SIZE;
   glyphAtlas->textureUnit = 0;
   glGenBuffers(1, &glyphAtlas->textureBufferObject);
   glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->textureBufferObject);
   glBufferData(GL_TEXTURE_BUFFER, glyphAtlas->capacityBytes, NULL, GL_STATIC_DRAW);

   glActiveTexture(GL_TEXTURE0 + (u32) glyphAtlas->textureUnit);
   glGenTextures(1, &glyphAtlas->texture);
   glBindTexture(GL_TEXTURE_BUFFER, glyphAtlas->texture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, glyphAtlas->textureBufferObject);
}

void _fmAtlasDeInit()
{
   struct GlyphAtlas *glyphAtlas = &E.fm.glyphAtlas;
   glDeleteBuffers(1, &glyphAtlas->textureBufferObject);
   glDeleteTextures(1, &glyphAtlas->texture);
}
