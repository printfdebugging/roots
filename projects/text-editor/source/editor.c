#include "editor.h"

#include "utils/macros.h"
#include "utils/constants.h"

#include "glad/glad.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool Run()
{
   const char *path = SOURCE_DIR "source/editor.c";

   if (!E.initialized)
      perror("E not initialized\n");
   if (!LoadTextFile(path))
      perror("Failed to load Text file\n");

   Update(EDITOR_STARTUP, (union UpdateState) {});

   while (!ShouldClose())
   {
      CalcFrameTime();
      glfwPollEvents();

      Layout();
      Upload();
      Render();
   }

   return true;
}

bool ShouldClose()
{
   if (!E.initialized)
      return true;

   bool shouldClose = true;
   shouldClose &= glfwWindowShouldClose(E.window);

   return shouldClose;
}

bool Init()
{
   if (E.initialized)
      return true;

   E.fontSize = DEFAULT_FONT_SIZE;
   if (!(E.fontFilePath = StringDuplicate(DEFAULT_FONT_FILE_PATH)))
      return false;

   bool windowExists = CreateGLFWwindow(
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

   FontMgrInit(E.fontFilePath);
   CreateTextShader(E.lineShader);
   InitBufferRenderer(E.bufRenderer, E.lineShader);
   glBufferData(GL_ARRAY_BUFFER, E.verticesCount * VERTEX_SIZE, NULL, GL_STATIC_DRAW);

   for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
      E.layoutMap[lineIdx].textLineIdx = lineIdx;

   E.initialized = true;
   return true;
}

void CalcFrameTime()
{
   double tNow = glfwGetTime();
   E.tDelta = tNow - E.tLast;
   E.tLast = tNow;
}

bool DeInit()
{
   DestroyTextShader(E.lineShader);
   DeInitBufferRenderer(E.bufRenderer);
   FontMgrDeInit();

   TextDestroy(E.text);
   free(E.text);

   free(E.lineShader);
   free(E.fontFilePath);
   free(E.vertices);

   glfwDestroyWindow(E.window);
   glfwTerminate();

   return true;
}

void Render()
{
   RenderBuffer();
}

void Upload()
{
   if (!E.layoutMapState.uploaded)
   {
      UploadBuffer();
      E.layoutMapState.uploaded = true;
   }
}

void UploadBuffer()
{
   if (E.verticesCount == 0)
      return;

   glBindVertexArray(E.bufRenderer->vao);
   glBindBuffer(GL_ARRAY_BUFFER, E.bufRenderer->vbo);

   for (uint32_t idx = 0; idx < LINES; ++idx)
   {
      if (E.layoutMap[idx].layouted && !E.layoutMap[idx].uploaded)
      {
         uint32_t offset = CHARS * VERTICES * idx;
         uint32_t byteOffset = offset * VERTEX_SIZE;
         uint32_t count = CHARS * VERTICES * VERTEX_SIZE;
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
void Update(enum UpdateEvent event, union UpdateState state)
{
   uint32_t lineCount = TextGetLineCount(E.text);
   uint32_t oldCurLine = E.cursorLine;
   uint32_t oldCurCol = E.cursorColumn;

   /* todo: later: this has update + layouting which is not the right shape to hold.
    * something has to be done, cursor position has to be updated for sure before the
    * second half of this function runs which is about layouting. */
   switch (event)
   {
      /* layouting */
      case EDITOR_STARTUP:
      {
         E.cursorColumn = 0;
         E.cursorLine = 0;

         for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
         {
            E.layoutMap[lineIdx] = (struct LayoutMap) {
               .textLineIdx = lineIdx,
               .layouted = false,
               .uploaded = false,
            };
         }

         break;
      }
      case KEY_PRESS:
      {
         /* later: we don't consider the count etc and we should do that. we should
          * also make sure that we are marking something as "needs cleanup for the
          * rest of the empty quads" */
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
               uint32_t lineLen = TextGetLineLength(E.text, E.cursorLine);
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
               uint32_t lineLen = TextGetLineLength(E.text, E.cursorLine);
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

               uint32_t lineLen = TextGetLineLength(E.text, E.cursorLine);
               if (E.cursorColumn < lineLen - 1)
                  E.cursorColumn += 1;

               break;
            }
            case GLFW_KEY_HOME:
            {
               E.cursorColumn = 0;
               break;
            }
            case GLFW_KEY_END:
            {
               uint32_t length = TextGetLineLength(E.text, E.cursorLine);
               if (length > 0)
                  E.cursorColumn = length - 1;
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
         LOG_INFO("E.lineOffset (%i) + LINES (%i) <= E.cursorLine (%i)\n", E.lineOffset, LINES, E.cursorLine);
         for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
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
         for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
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
      LOG_INFO("E.cursorColumn (%i) != oldCurCol (%i)\n", E.cursorColumn, oldCurCol);
      if (E.columnOffset + CHARS <= E.cursorColumn) /* cursor move right */
      {
         LOG_INFO("E.columnOffset (%i) + CHARS (%i) < E.cursorColumn (%i)\n", E.columnOffset, CHARS, E.cursorColumn)
         for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
            E.layoutMap[lineIdx].layouted = false;

         uint32_t lastVisColIdx = E.columnOffset + CHARS - 1;
         E.columnOffset += (E.cursorColumn - lastVisColIdx);
      }
      else if (E.cursorColumn < E.columnOffset) /* cursor move left */
      {
         LOG_INFO("E.cursorColumn (%i) < E.columnOffset (%i)\n", E.cursorColumn, E.columnOffset)
         for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
            E.layoutMap[lineIdx].layouted = false;
         E.columnOffset -= (E.columnOffset - E.cursorColumn);
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

void Layout()
{
   if (!E.layoutMapState.layouted)
   {
      LayoutBuffer();
      E.layoutMapState.layouted = true;
      E.layoutMapState.uploaded = false;
   }
}

/**!
 * Loads the text file from `filePath` into a `Text` object,
 * and returns an index to it, or `INVALID_ID` on error.
 */
bool LoadTextFile(const char *filePath)
{
   if (!filePath)
      return false;
   if (!(E.text = TextLoadFromFile(filePath)))
      return false;
   return true;
}

/* warn: todo: add cleanup at some later stage when it works */
/* returns a buffer id.. todo: write nicely later, let's first make it work */
void EditorOpenFile(const char *path)
{
   (void) path;
   /* todo: */
}

/* todo: remove editor from here */
void RenderBuffer()
{
   struct GlyphAtlas *atlas = FontMgrGetAtlas();
   struct Rectangle bounds = GetWindowBounds();

   mat4s mvp = { GLM_MAT4_IDENTITY_INIT };
   mvp = glms_ortho(0, (float) bounds.w, 0, (float) bounds.h, 0.0f, 100.0f);
   mvp = glms_translate(mvp, (vec3s) { { 0.0f, 0.0f, 0.0f } }); /* not set as of now */

   ivec4s viewport = { 0 };
   glGetIntegerv(GL_VIEWPORT, viewport.raw);

   glClearColor(ColorRGBAHex(0X002b36FF));
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   float fontScale = FontMgrGetDefaultFontScale();

   E.bufRenderer->uniforms = (struct TextShaderUniforms) {
      .matViewProjection = mvp,
      .viewport = viewport,
      .scale = fontScale,
      .hbGpuAtlas = atlas->textureUnit,
      .gamma = 1.0f,
      .debug = false,
      .stemDarkening = false,
   };

   UploadTextShaderUniforms(E.lineShader, &E.bufRenderer->uniforms);

   if (E.bufRenderer->uploaded)
   {
      glBindVertexArray(E.bufRenderer->vao);
      glDrawArrays(GL_TRIANGLES, 0, (int32_t) E.verticesCount);
   }

   /* note: not sure if this should be done after each buffer is rendered, or after all of them
    * are rendered. for now we just do it here since we only have a single buffer. */
   SwapGLBuffers();
}

struct GlyphInfo *_glyphInfo = NULL;

void LayoutBuffer()
{
   if (!E.lineShader || !E.text || !E.window)
      return;

   float lineHeight = FontMgrGetDefaultFontLineHeight();
   float fontScale = FontMgrGetDefaultFontScale();

   struct Rectangle bounds = GetWindowBounds();
   for (uint32_t visLineIdx = 0; visLineIdx < LINES; ++visLineIdx)
   {
      if (E.layoutMap[visLineIdx].layouted)
         continue;

      /**!
       * scale = #pixels one point represents
       * points * scale = pixels
       * pixels / scale = points
       */
      vec2s linePos = { .x = 0, .y = ((float) bounds.h - ((float) (visLineIdx + 1) * lineHeight)) / fontScale };

      struct Font *font = FontMgrGetDefaultFont();
      hb_buffer_t *buffer = hb_buffer_create();

      struct StringView view = TextGetLineUTF8AtOffset(E.text, E.layoutMap[visLineIdx].textLineIdx, E.columnOffset);
      hb_buffer_add_utf8(buffer, view.data, (int32_t) view.count, 0, -1);
      hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
      hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
      hb_shape(font->hbFont, buffer, NULL, 0);

      uint32_t hbGlyphCount = 0;
      hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &hbGlyphCount);

      _glyphInfo = realloc(_glyphInfo, hbGlyphCount * sizeof(struct GlyphInfo));
      if (_glyphInfo)
         memset(_glyphInfo, 0, hbGlyphCount * sizeof(struct GlyphInfo));

      uint32_t glyphCount = hbGlyphCount < CHARS ? hbGlyphCount : CHARS;

      for (uint32_t glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx)
      {
         hb_codepoint_t glyphIndex = glyphInfos[glyphIdx].codepoint;
         FontMgrCacheGlyphInfo(font, glyphIndex);
         _glyphInfo[glyphIdx] = font->glyphCache[glyphIndex];
      }

      hb_buffer_destroy(buffer);

      struct Point glyphPosition = {
         .x = linePos.x,
         .y = linePos.y,
      };

      /* we loop over the available slots */
      for (uint32_t glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx)
      {
         [[maybe_unused]] bool hasCursor;
         struct GlyphInfo *glyphInfo = &_glyphInfo[glyphIdx];

         glyphPosition.x += glyphInfo->extents.xMin;
         glyphPosition.y += 0;

         struct GlyphVertex glyphQuadCorners[4];

         uint32_t visualColumn = 0;
         if (E.cursorColumn - E.columnOffset >= CHARS)
            visualColumn = CHARS - 1;
         else if (E.cursorColumn < E.columnOffset)
            visualColumn = 0;
         else
            visualColumn = E.cursorColumn - E.columnOffset;

         /* LOG_INFO(
             "hasCursor: %i, E.cursorLine (%i) == E.layoutMap[visLineIdx].textLineIdx (%i) && visualColumn (%i) == glyphIdx (%i)\n",
             (E.cursorLine == E.layoutMap[visLineIdx].textLineIdx && visualColumn == glyphIdx),
             E.cursorLine,
             E.layoutMap[visLineIdx].textLineIdx,
             visualColumn,
             glyphIdx
         ) */

         uint32_t lineOffset = visLineIdx * CHARS * VERTICES;
         for (int cornerIdx = 0; cornerIdx < 4; cornerIdx++)
         {
            int32_t cx = (cornerIdx >> 1) & 1;
            int32_t cy = cornerIdx & 1;
            double ex = (1 - cx) * glyphInfo->extents.xMin + cx * glyphInfo->extents.xMax;
            double ey = (1 - cy) * glyphInfo->extents.yMin + cy * glyphInfo->extents.yMax;

            glyphQuadCorners[cornerIdx] = (struct GlyphVertex) {
               .x = (float) glyphPosition.x,
               .y = (float) glyphPosition.y,
               .tx = (float) ex,
               .ty = (float) ey,
               .nx = cx ? 1.f : -1.f,
               .ny = cy ? -1.f : 1.f,
               .emPerPos = 1.0,
               .atlasOffset = glyphInfo->atlasOffset / TEXEL_SIZE,
               .fgColor = (vec4s) { { ColorRGBAHex(0X839496FF) } },
               .bgColor = (vec4s) { { ColorRGBAHex(0X000000FF) } },
               .hasCursor = (E.cursorLine == E.layoutMap[visLineIdx].textLineIdx && visualColumn == glyphIdx),
            };
         }

         uint32_t glyphOffset = glyphIdx * VERTICES;
         uint32_t glyphQuadOffset = lineOffset + glyphOffset;
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

      /* this would do for now */
      if (glyphCount < CHARS)
      {
         uint32_t lineOffset = (visLineIdx * CHARS * VERTICES);
         for (uint32_t glyphIdx = glyphCount; glyphIdx < CHARS; ++glyphIdx)
         {
            uint32_t glyphOffset = glyphIdx * VERTICES;
            uint32_t glyphQuadOffset = lineOffset + glyphOffset;

            E.vertices[glyphQuadOffset + 0] = (struct GlyphVertex) { 0 };
            E.vertices[glyphQuadOffset + 1] = (struct GlyphVertex) { 0 };
            E.vertices[glyphQuadOffset + 2] = (struct GlyphVertex) { 0 };
            E.vertices[glyphQuadOffset + 3] = (struct GlyphVertex) { 0 };
            E.vertices[glyphQuadOffset + 4] = (struct GlyphVertex) { 0 };
            E.vertices[glyphQuadOffset + 5] = (struct GlyphVertex) { 0 };
         }
      }

      E.layoutMap[visLineIdx].layouted = true;
      E.layoutMap[visLineIdx].uploaded = false;
   }
}

bool CreateGLFWwindow(struct GLFWwindowOptions opts)
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

   const int32_t windowWidth = opts.width ? opts.width : 1600;
   const int32_t windowHeight = opts.height ? opts.height : 800;
   const char *windowTitle = opts.title ? opts.title : "GLFWwindow";

   /* todo: re-implement it later */
   GLFWwindow *sharedWindow = NULL;

   GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, sharedWindow);
   if (!window)
      return false;

   const int32_t maxWidth = 2230;
   const int32_t maxHeight = 1420;
   const int32_t minWidth = 800;
   const int32_t minHeight = 600;

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

struct Rectangle GetWindowBounds()
{
   if (!E.window)
      return (struct Rectangle) {};

   int32_t windowWidth, windowHeight;
   glfwGetWindowSize(E.window, &windowWidth, &windowHeight);

   return (struct Rectangle) {
      .x = 0,
      .y = 0,
      .w = windowWidth,
      .h = windowHeight,
   };
}

void SwapGLBuffers()
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

void fbResizeFn(GLFWwindow *window, int32_t width, int32_t height)
{
   (void) window;
   glViewport(0, 0, width, height);
}

void scrollFn(GLFWwindow *window, double x, double y)
{
   (void) window;
   (void) x;
   (void) y;
}

void curPosFn(GLFWwindow *window, double x, double y)
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
      Update(KEY_PRESS, (union UpdateState) { .glfwKey = key });
}
