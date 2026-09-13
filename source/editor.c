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
   [[maybe_unused]] i32 bufId = openFile(ASSETS_DIR "test.md");

   while (!shouldClose())
   {
      calcFrameTime();
      glfwPollEvents();
      render();
   }

   return true;
}

bool shouldClose()
{
   if (!E.initialized)
      return true;

   bool shouldClose = true;
   for (i32 winId = 0; winId < (i32) E.windowCount; ++winId)
      if (winId != E.sharedWindowId)
         shouldClose &= glfwWindowShouldClose(E.window[winId]);

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
   E.sharedWindowId = createWindow(
       (struct GLFWwindowOptions) {
          .width       = 800,
          .height      = 600,
          .title       = "GLFWwindow",
          .transparent = false,
          .visible     = false,
          .icon        = DEFAULT_WINDOW_ICON,
          .sharedWinId = INVALID_ID,
          .fbResizeFn  = fbResizeFn,
          .keyFn       = keyFn,
          .scrollFn    = scrollFn,
          .curPosFn    = curPosFn,
       }
   );

   glfwSetErrorCallback(_glfwErrFn);

   fmInit(E.fontFilePath);
   if (!(E.lineShader = calloc(1, sizeof(struct LineShader))))
      return false;
   lineShaderInit(E.lineShader);

   E.initialized = true;
   return true;
}

void calcFrameTime()
{
   f64 tNow = glfwGetTime();
   E.tDelta = tNow - E.tLast;
   E.tLast  = tNow;
}

bool deInit()
{
   for (i32 idx = 0; idx < E.textCount; ++idx)
      textDestroy(E.text[idx]);
   for (i32 idx = 0; idx < E.textCount; ++idx)
      free(E.text[idx]);

   for (i32 idx = 0; idx < E.lineRendererCount; ++idx)
      lineRendererDeInit(E.lineRenderer[idx]);
   for (i32 idx = 0; idx < E.lineRendererCount; ++idx)
      free(E.lineRenderer[idx]);

   for (i32 idx = 0; idx < E.bufferCount; ++idx)
   {
      free(E.textBuffer[idx]->visLineRenderers);
      free(E.textBuffer[idx]);
      free(E.textBuffer);
   }

   /**!
    * note: Till we have a shared hidden window which
    * is destroyed at the end, we need to do this last
    */
   for (i32 idx = 0; idx < E.windowCount; ++idx)
      glfwDestroyWindow(E.window[idx]);

   /* note: todo: maybe this should be above the window destruction sequence */
   lineShaderDeInit(E.lineShader);
   fmDeInit();

   free(E.text);
   free(E.window);
   free(E.lineRenderer);
   free(E.lineShader);
   free(E.fontFilePath);
   glfwTerminate();

   return true;
}

void render()
{
   for (i32 bufId = 0; bufId < E.bufferCount; ++bufId)
      drawBuffer(bufId);
}

/**!
 * Loads the text file from `filePath` into a `Text` object,
 * and returns an index to it, or `INVALID_ID` on error.
 */
i32 loadTextFile(const char *filePath)
{
   if (!filePath)
      return INVALID_ID;

   struct Text *text = textLoadFromFile(filePath);
   if (!text)
      return INVALID_ID;

   E.text = realloc(E.text, sizeof(struct Text *) * ((u32) E.textCount + 1));
   if (!E.text)
      return INVALID_ID;

   E.text[E.textCount] = text;
   return (i32) E.textCount++;
}

/* warn: todo: add cleanup at some later stage when it works */
/* returns a buffer id.. todo: write nicely later, let's first make it work */
i32 openFile(const char *path)
{
   struct Buffer *buf = NULL;
   i32 textId;

   if (!E.initialized)
      goto failure;
   if ((textId = loadTextFile(path)) == INVALID_ID)
      goto failure;
   if (!(buf = calloc(1, sizeof(struct Buffer))))
      goto failure;

   *buf = (struct Buffer) {
      .winId            = INVALID_ID,
      .txtId            = textId,
      .editor           = NULL,
      .cursorColumn     = 0,
      .cursorLine       = 0,
      .hOffset          = 0,
      .vOffset          = 0,
      .visLineRenderers = NULL,
      .visLineCount     = 0,
   };

   buf->winId = createWindow(
       (struct GLFWwindowOptions) {
          .width       = 800,
          .height      = 600,
          .title       = "GLFWwindow",
          .transparent = true,
          .visible     = true,
          .icon        = DEFAULT_WINDOW_ICON,
          .sharedWinId = E.sharedWindowId,
          .fbResizeFn  = fbResizeFn,
          .keyFn       = keyFn,
          .scrollFn    = scrollFn,
          .curPosFn    = curPosFn,
       }
   );

   /* todo: put them behind wrappers */
   glActiveTexture(GL_TEXTURE0 + (u32) E.fm.glyphAtlas.textureUnit);
   glBindTexture(GL_TEXTURE_BUFFER, E.fm.glyphAtlas.texture);
   glUseProgram(E.lineShader->hbShaderProgram);

   struct Text *text = E.text[textId];
   u32 lineCount     = textGetLineCount(text);

   /* todo: allocate these just for the visible lines, not for all the lines. */
   if (!(buf->visLineRenderers = calloc(lineCount, sizeof(i32))))
      goto failure;

   memset(buf->visLineRenderers, INVALID_ID, sizeof(i32) * lineCount);

   for (u32 lineIdx = 0; lineIdx < lineCount; ++lineIdx)
   {
      /* todo: a line should hold a text id and a line number
       * just to be more aware of where it is coming from.. */
      buf->visLineRenderers[lineIdx] = createLine(
          &E,
          (struct LineOptions) {
             .lineIdx = lineIdx,
             .textId  = textId,
          }
      );
   }

   if (!(E.textBuffer = realloc(E.textBuffer, sizeof(struct Buffer *) * ((u32) E.bufferCount + 1))))
      goto failure;

   E.textBuffer[E.bufferCount] = buf;
   return (i32) E.bufferCount++;

failure:
   if (buf)
      free(buf->visLineRenderers);
   free(buf);

   return INVALID_ID;
}

/* todo: remove editor from here */
void drawBuffer(i32 bufId)
{
   /* todo: move to a new api */
   /* todo: fix this with new API over IDs */
   GLFWwindow *window = E.window[E.textBuffer[bufId]->winId];
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
   struct Font *font = fmGetDefaultFont();
   hb_font_get_scale(font->hbFont, &xScale, &yScale);
   f32 fontScale = E.fontSize / (f32) yScale;

   struct GlyphAtlas *atlas = fmGetAtlas();

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

   struct Buffer *buf = E.textBuffer[bufId];

   glClearColor(ColorRGBAHex(0X002b36FF));

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   struct Text *text = E.text[buf->txtId];
   u32 lineCount     = textGetLineCount(text);

   buf->visLineCount = (u32) windowHeight / (u32) lineHeight;
   if (lineCount < buf->visLineCount)
      buf->visLineCount = lineCount;

   for (u32 lineIdx = 0; lineIdx < buf->visLineCount; ++lineIdx)
   {
      i32 lineId = buf->visLineRenderers[lineIdx];

      struct LineRenderer *lineRenderer = E.lineRenderer[lineId];

      lineRenderer->uniforms = (struct LineShaderUniforms) {
         .matViewProjection = mvp,
         .viewport          = viewport,
         .scale             = fontScale,
         .position          = { .x = 0, .y = ((f32) windowHeight - ((f32) lineHeight * ((f32) lineIdx + 1))) },
         .hbGpuAtlas        = atlas->textureUnit,
         .gamma             = 1.0f,
         .debug             = false,
         .stemDarkening     = false,
      };

      lineShaderUploadUniforms(E.lineShader, &lineRenderer->uniforms);

      if (lineRenderer->uploaded)
      {
         glBindVertexArray(lineRenderer->vao);
         glDrawArrays(GL_TRIANGLES, 0, (i32) lineRenderer->count);
      }
   }

   glfwSwapBuffers(window);
}

i32 createWindow(struct GLFWwindowOptions opts)
{
   if (!glfwInit())
      return INVALID_ID;

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

   const i32 windowWidth   = opts.width ? opts.width : 1600;
   const i32 windowHeight  = opts.height ? opts.height : 800;
   const char *windowTitle = opts.title ? opts.title : "GLFWwindow";

   GLFWwindow *sharedWindow = NULL;
   if (opts.sharedWinId != INVALID_ID && opts.sharedWinId < (i32) E.windowCount)
      sharedWindow = E.window[opts.sharedWinId];

   GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, sharedWindow);
   if (!window)
      return INVALID_ID;

   glfwMakeContextCurrent(window);
   gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
   glfwSwapInterval(1);

#ifndef __APPLE__
   GLFWimage img;
   int chanCount;
   opts.icon  = opts.icon ? opts.icon : DEFAULT_WINDOW_ICON;
   img.pixels = stbi_load(opts.icon, &img.width, &img.height, &chanCount, 0);

   if (!img.pixels)
   {
      glfwDestroyWindow(window);
      return INVALID_ID;
   }

   glfwSetWindowIcon(window, 1, &img);
   free(img.pixels);
#endif

#ifdef _WIN32
   HWND hwnd   = glfwGetWin32Window(window);
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
      return INVALID_ID;

   E.window = realloc(E.window, sizeof(GLFWwindow *) * ((u32) E.windowCount + 1));
   if (!E.window)
      return INVALID_ID;

   E.window[E.windowCount] = window;
   return (i32) E.windowCount++;
}

/**!
 * A sane argument against this per line approach is to do it for
 * all the visible lines at the same time. That makes sense, though
 * is not actionable at the moment as that would require doing many
 * things at the same time.. so listing that as a todo: here.
 *
 * For now this works because we would have to relayout each line as they
 * are marked dirty and there are less things to manage here.. But as
 * it gets in shape, we would move these to one large function, maybe...
 * intuition says we would still need to keep the per line thing..
 */
i32 createLine(struct Editor *editor, struct LineOptions opts)
{
   if (!editor->lineShader)
      return INVALID_ID;
   if (opts.textId == INVALID_ID /*  && !opts.isVirtual */)
      return INVALID_ID;

   struct LineRenderer *renderer = calloc(1, sizeof(struct LineRenderer));
   if (!renderer)
      return INVALID_ID;

   lineRendererInit(renderer, editor->lineShader);

   /* todo: hide strlen behind the text api so that we can later replace it with something more efficient. */
   struct Text *text = editor->text[opts.textId];
   char *lineBytes   = textGetUTF8Line(text, opts.lineIdx);
   u64 lineByteLen   = strlen(lineBytes);
   fmLayoutLine(renderer, lineBytes, lineByteLen);

   if (!(editor->lineRenderer = realloc(editor->lineRenderer, sizeof(struct LineRenderer *) * ((u32) editor->lineRendererCount + 1))))
   {
      lineRendererDeInit(renderer);
      free(renderer);
      return INVALID_ID;
   }

   editor->lineRenderer[editor->lineRendererCount] = renderer;
   return (i32) editor->lineRendererCount++;
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

struct GlyphInfo *_glyphInfo = NULL;

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
   E.fm.editorFont     = fmGetFont(editorFontPath);
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

void fmLayoutLine(struct LineRenderer *renderer, char *lineUTF8, u64 lineByteLen)
{
   (void) lineByteLen;
   if (!E.fm.initialized)
      return;

   struct Font *font = fmGetDefaultFont();

   hb_buffer_t *buffer = hb_buffer_create();
   hb_buffer_add_utf8(buffer, lineUTF8, -1, 0, -1);
   hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
   hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
   hb_shape(font->hbFont, buffer, NULL, 0);

   u32 hbGlyphCount            = 0;
   hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &hbGlyphCount);

   _glyphInfo = realloc(_glyphInfo, hbGlyphCount * sizeof(struct GlyphInfo));
   memset(_glyphInfo, 0, hbGlyphCount * sizeof(struct GlyphInfo));

   for (u32 glyphIdx = 0; glyphIdx < hbGlyphCount; ++glyphIdx)
   {
      hb_codepoint_t glyphIndex = glyphInfos[glyphIdx].codepoint;
      struct GlyphInfo *glyph   = &font->glyphCache[glyphIndex];
      if (!glyph->cached)
      {
         i32 xScale, yScale;
         hb_font_get_scale(font->hbFont, &xScale, &yScale);
         hb_gpu_draw_clear(font->hbDraw);
         hb_gpu_draw_glyph(font->hbDraw, font->hbFont, glyphIndex);

         hb_glyph_extents_t hbGlyphExtents = {};
         hb_blob_t *hbBlob                 = NULL;

         hbBlob           = hb_gpu_draw_encode(font->hbDraw, &hbGlyphExtents);
         u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

         *glyph = (struct GlyphInfo) {
            .extents.xMin = 0,
            .extents.xMax = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
            .extents.yMin = font->hbDescent,
            .extents.yMax = font->hbAscent,
            .advance      = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
            .upem         = yScale,
            .empty        = (hbBlobLength == 0),
            .cached       = true,
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

   renderer->count    = hbGlyphCount * 6;
   renderer->vertices = realloc(renderer->vertices, renderer->count * sizeof(struct GlyphVertex));

   struct Point glyphPosition = { .x = 0, .y = 0 };
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
            .x           = (f32) glyphPosition.x,
            .y           = (f32) glyphPosition.y,
            .tx          = (f32) ex,
            .ty          = (f32) ey,
            .nx          = cx ? 1.f : -1.f,
            .ny          = cy ? -1.f : 1.f,
            .emPerPos    = 1.0,
            .atlasOffset = glyphInfo->atlasOffset / TEXEL_SIZE,
            .hasCursor   = false,
            .fgColor     = (vec4s) { { ColorRGBAHex(0X839496FF) } },
            .bgColor     = (vec4s) { { ColorRGBAHex(0X000000FF) } },
            /* next: fix this. for now, nothing has a cursor */
         };
      }

      u32 glyphQuadOffset = glyphIdx * 6;

      renderer->vertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
      renderer->vertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
      renderer->vertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
      renderer->vertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
      renderer->vertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
      renderer->vertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

      glyphPosition.x += glyphInfo->extents.xMax;
      glyphPosition.y += 0;
   }

   glBindVertexArray(renderer->vao);
   glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * renderer->count, renderer->vertices, GL_STATIC_DRAW);
   renderer->count    = renderer->count;
   renderer->uploaded = true;
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

   E.fm.font         = realloc(E.fm.font, sizeof(struct Font) * (E.fm.fontCount + 1));
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

struct Font *fmGetFontWithRune(rune codepoint)
{
   (void) codepoint;
   perror("todo");
   return NULL;
}

void fntInit(struct Font *font, const char *filePath)
{
   font->fontPath   = stringDuplicate(filePath);
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

   const hb_ot_metrics_tag_t ASCENT_HHEA  = HB_TAG('H', 'a', 's', 'c');
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

   glyphAtlas->capacityBytes     = ATLAS_PAGE_SIZE;
   glyphAtlas->cursorOffsetBytes = TEXEL_SIZE;
   glyphAtlas->textureUnit       = 0;
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
