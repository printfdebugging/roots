#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"
#include "stb_image.h"
#include "hb-ot.h"

#include "editor.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <dwmapi.h>
#include "GLFW/glfw3native.h"
#endif

static struct Editor E = { 0 };

void _glfwErrFn(int code, const char *description);

#ifdef _WIN32
static bool _msIsDarkMode();
#endif

bool run()
{
   const char *path = ASSETS_DIR "mini-test.md";
   if (!E.initialized)
      perror("E not initialized\n");
   if (!loadTextFile(path))
      perror("Failed to load Text file\n");

   update(EDITOR_STARTUP, (union UpdateState) {});

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

   bool windowExists = createWindow(
       (struct GLFWwindowOptions) {
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
       }
   );

   if (!windowExists)
      perror("failed to create a window");

   glfwSetErrorCallback(_glfwErrFn);

   if (!(E.lineShader = calloc(1, sizeof(struct TextShader))))
      return false;
   if (!(E.bufRenderer = calloc(1, sizeof(struct BufferRenderer))))
      return false;

   E.verticesCount = CHARS * LINES * VERTICES;
   if (!(E.vertices = calloc(E.verticesCount, VERTEX_SIZE)))
      return false;

   fmInit(E.fontFilePath);
   createTextShader(E.lineShader);
   initBufferRenderer(E.bufRenderer, E.lineShader);
   glBufferData(GL_ARRAY_BUFFER, E.verticesCount * VERTEX_SIZE, NULL, GL_STATIC_DRAW);

   for (u32 lineIdx = 0; lineIdx < LINES; ++lineIdx)
      E.layoutMap[lineIdx].textLineIdx = lineIdx;

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
   destroyTextShader(E.lineShader);
   deInitBufferRenderer(E.bufRenderer);
   fmDeInit();

   textDestroy(E.text);
   free(E.text);

   free(E.lineShader);
   free(E.fontFilePath);
   free(E.vertices);

   glfwDestroyWindow(E.window);
   glfwTerminate();

   return true;
}

void render()
{
   renderBuffer();
}

void upload()
{
   if (!E.layoutMapState.uploaded)
   {
      uploadBuffer();
      E.layoutMapState.uploaded = true;
   }
}

void uploadBuffer()
{
   if (E.verticesCount == 0)
      return;

   glBindVertexArray(E.bufRenderer->vao);
   glBindBuffer(GL_ARRAY_BUFFER, E.bufRenderer->vbo);

   for (u32 idx = 0; idx < LINES; ++idx)
   {
      if (E.layoutMap[idx].layouted && !E.layoutMap[idx].uploaded)
      {
         u32 offset = CHARS * VERTICES * idx;
         u32 byteOffset = offset * VERTEX_SIZE;
         u32 count = CHARS * VERTICES * VERTEX_SIZE;
         glBufferSubData(GL_ARRAY_BUFFER, byteOffset, count, E.vertices + offset);
         E.layoutMap[idx].uploaded = true;
      }
   }

   E.bufRenderer->uploaded = true;
}

/*
 * This should prepare the layoutMap such that
 * layout can just be sure that that's up to date and just loop over
 * it and either relayout or skip.
 *
 * This should be triggered by events like cursor moved, or window resized, etc etc..
 * This should probably take a "who calls the update and for what" param
 */
void update(enum UpdateEvent event, union UpdateState state)
{
   u32 lineCount = textGetLineCount(E.text);
   u32 oldCurLine = E.cursorLine;
   u32 oldCurCol = E.cursorColumn;

   switch (event)
   {
      case EDITOR_STARTUP:
      {
         break;
      }
      case KEY_PRESS:
      {
         switch (state.glfwKey)
         {
            /* this crashes the application when key is clicked */
            case GLFW_KEY_DOWN:
            {
               LOG_EVENT("update: GLFW_KEY_DOWN\n");

               bool alreadyOnTheLastLine = E.cursorLine == lineCount - 1;
               if (alreadyOnTheLastLine)
                  break;

               E.cursorLine += 1;

               /* dup */
               u32 lineLen = textGetLineLength(E.text, E.cursorLine);
               if (lineLen < E.cursorColumn)
                  E.cursorColumn = lineLen - 1;

               break;
            }
            case GLFW_KEY_UP:
            {
               LOG_EVENT("update: GLFW_KEY_UP\n");

               /* already on the first line */
               bool alreadyOnTheFirstLine = E.cursorLine == 0;
               if (alreadyOnTheFirstLine)
                  break;

               E.cursorLine -= 1;

               /* dup */
               u32 lineLen = textGetLineLength(E.text, E.cursorLine);
               if (lineLen < E.cursorColumn)
                  E.cursorColumn = lineLen - 1;

               break;
            }
            case GLFW_KEY_LEFT:
            {
               LOG_EVENT("update: GLFW_KEY_LEFT\n");

               bool alreadyOnTheFirstColumn = E.cursorColumn == 0;
               if (alreadyOnTheFirstColumn)
                  break;

               E.cursorColumn -= 1;

               break;
            }
            case GLFW_KEY_RIGHT:
            {
               LOG_EVENT("update: GLFW_KEY_RIGHT\n");

               u32 lineLen = textGetLineLength(E.text, E.cursorLine);
               if (E.cursorColumn < lineLen - 1)
                  E.cursorColumn += 1;

               break;
            }
         }
         break;
      }
   }

   /*
    * note: Line calculations should happen before column calculations, because columns
    * depend on lines and it's possible (locally observed) that line changes might
    * change some offsets which the column tests need, and without those chagnes, the indices
    * are wrong and all the sudden you get a crash.
    */

   if (E.cursorLine != oldCurLine)
   {
      if (E.lineOffset + LINES <= E.cursorLine)
      {
         LOG_INFO("E.lineOffset (%i) + LINES (%i) < E.cursorLine (%i)\n", E.lineOffset, LINES, E.cursorLine);
         for (u32 lineIdx = 0; lineIdx < LINES; ++lineIdx)
         {
            /* todo: note: this should be shifting rather than just blind increment */
            E.layoutMap[lineIdx].layouted = false;
            E.layoutMap[lineIdx].textLineIdx += 1;
         }
         E.lineOffset++;
      }
      else if (E.cursorLine < E.lineOffset)
      {
         LOG_INFO("E.cursorLine (%i) < E.lineOffset (%i)\n", E.cursorLine, E.lineOffset);
         for (u32 lineIdx = 0; lineIdx < LINES; ++lineIdx)
         {
            /* todo: note: this should be shifting rather than just blind increment */
            E.layoutMap[lineIdx].layouted = false;
            E.layoutMap[lineIdx].textLineIdx -= 1;
         }
         E.lineOffset--;
      }
      else
      {
         LOG_INFO("E.layoutMap[E.cursorLine (%i) - E.lineOffset (%i)].layouted (%i) = false;\n", E.cursorLine, E.lineOffset, E.layoutMap[E.cursorLine - E.lineOffset].layouted);
         E.layoutMap[oldCurLine - E.lineOffset].layouted = false;
         E.layoutMap[E.cursorLine - E.lineOffset].layouted = false;
      }

      E.layoutMapState.updated = true;
      E.layoutMapState.layouted = false;

      /* if scroll past the edges, then all lines relayout. middle ones just move one step up */
      /* if scroll within visible range, invalidate both lines. later with a cursor moved flag */
   }

   if (E.cursorColumn != oldCurCol)
   {
      if (oldCurCol + CHARS < E.cursorColumn) /* cursor move right */
      {
         LOG_INFO("oldCurCol (%i) + CHARS (%i) < E.cursorColumn (%i)\n", oldCurCol, CHARS, E.cursorColumn)
         for (u32 lineIdx = 0; lineIdx < LINES; ++lineIdx)
            E.layoutMap[lineIdx].layouted = false;
         E.columnOffset++;
      }
      else if (E.cursorColumn < E.columnOffset) /* cursor move left */
      {
         LOG_INFO("E.cursorColumn (%i) < E.columnOffset (%i)\n", E.cursorColumn, E.columnOffset)
         for (u32 lineIdx = 0; lineIdx < LINES; ++lineIdx)
            E.layoutMap[lineIdx].layouted = false;
         E.columnOffset--;
      }
      else /* moved over visible columns */
      {
         LOG_INFO("E.layoutMap[E.cursorLine (%i) - E.lineOffset (%i)].layouted (%i) = false;\n", E.cursorLine, E.lineOffset, E.layoutMap[E.cursorLine - E.lineOffset].layouted);
         E.layoutMap[E.cursorLine - E.lineOffset].layouted = false;
      }

      E.layoutMapState.updated = true;
      E.layoutMapState.layouted = false;
   }
}

void layout()
{
   if (!E.layoutMapState.layouted)
   {
      layoutBuffer();
      E.layoutMapState.layouted = true;
      E.layoutMapState.uploaded = false;
   }
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
      glDrawArrays(GL_TRIANGLES, 0, (i32) E.verticesCount);
   }

   /* note: not sure if this should be done after each buffer is rendered, or after all of them
    * are rendered. for now we just do it here since we only have a single buffer. */
   swapBuffers();
}

struct GlyphInfo *_glyphInfo = NULL;

void layoutBuffer()
{
   if (!E.lineShader || !E.text || !E.window || !E.fm.initialized)
      return;

   f32 lineHeight = fmGetDefaultFontLineHeight();
   f32 fontScale = fmGetDefaultFontScale();

   struct Rectangle bounds = getWindowBounds();
   for (u32 visLineIdx = 0; visLineIdx < LINES; ++visLineIdx)
   {
      if (E.layoutMap[visLineIdx].layouted)
         continue;

      char *lineBytes = textGetUTF8Line(E.text, E.layoutMap[visLineIdx].textLineIdx);
      if (!lineBytes)
         continue;

      /**!
       * scale = #pixels one point represents
       * points * scale = pixels
       * pixels / scale = points
       */
      vec2s linePos = { .x = 0, .y = ((f32) bounds.h - ((f32) (visLineIdx + 1) * lineHeight)) / fontScale };

      struct Font *font = fmGetDefaultFont();
      hb_buffer_t *buffer = hb_buffer_create();
      hb_buffer_add_utf8(buffer, lineBytes, -1, 0, -1);
      hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
      hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
      hb_shape(font->hbFont, buffer, NULL, 0);

      u32 hbGlyphCount = 0;
      hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &hbGlyphCount);

      _glyphInfo = realloc(_glyphInfo, hbGlyphCount * sizeof(struct GlyphInfo));
      memset(_glyphInfo, 0, hbGlyphCount * sizeof(struct GlyphInfo));

      u32 glyphCount = hbGlyphCount < CHARS ? hbGlyphCount : CHARS;

      for (u32 glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx)
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

      struct Point glyphPosition = {
         .x = linePos.x,
         .y = linePos.y,
      };

      /* we loop over the available slots */
      for (u32 glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx)
      {
         [[maybe_unused]] bool hasCursor;
         struct GlyphInfo *glyphInfo = &_glyphInfo[glyphIdx];

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
               .hasCursor = (E.cursorLine == E.layoutMap[visLineIdx].textLineIdx && E.cursorColumn == glyphIdx),
               /* next: fix this. for now, nothing has a cursor */
            };
         }
         // LOG_INFO("E.cursorLine %i, E.layoutMap[visLineIdx].textLineIdx %i\n", E.cursorLine, E.layoutMap[visLineIdx].textLineIdx)

         u32 glyphQuadOffset = (glyphIdx * 6) + (visLineIdx * CHARS * VERTICES);
         E.vertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
         E.vertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
         E.vertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
         E.vertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
         E.vertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
         E.vertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

         /* note: todo: this currently assumes the layout to be horizontal, fine assumption
          * when starting out, but later we would also want to cater for the vertical
          * writing styles. */
         glyphPosition.x += glyphInfo->extents.xMax;
      }

      E.layoutMap[visLineIdx].layouted = true;
      E.layoutMap[visLineIdx].uploaded = false;
   }
}

bool createWindow(struct GLFWwindowOptions opts)
{
   if (!glfwInit())
      return false;

   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, opts.transparent);
   glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
   glfwWindowHint(GLFW_VISIBLE, opts.visible);
   glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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

   const i32 maxWidth = 2230;
   const i32 maxHeight = 1420;
   const i32 minWidth = 800;
   const i32 minHeight = 600;

   glfwSetWindowSizeLimits(window, minWidth, minHeight, maxWidth, maxHeight);
   glfwMakeContextCurrent(window);
   gladLoadGL((GLADloadfunc) glfwGetProcAddress);
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

   if (action == GLFW_PRESS || action == GLFW_REPEAT)
      update(KEY_PRESS, (union UpdateState) { .glfwKey = key });
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
