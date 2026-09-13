#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glad/glad.h"
#include "stb_image.h"

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

   // fontManagerInit(E.fontFilePath);
   if (!(E.lineShader = calloc(1, sizeof(struct LineShader))))
      return false;
   // lineShaderInit(E.lineShader);

   E.initialized = true;
   return true;
}

void calcFrameTime()
{
   f64 timeNow = glfwGetTime();
   E.timeDelta = timeNow - E.lastTime;
   E.lastTime  = timeNow;
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
   fontManagerDeInit();

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
      drawBuffer(&E, bufId);
}

/**!
 * Loads the text file from `filePath` into a `Text` object,
 * and returns an index to it, or `-1` on error.
 */
i32 loadTextFile(struct Editor *editor, const char *filePath)
{
   if (!filePath)
      return INVALID_ID;

   struct Text *text = textLoadFromFile(filePath);
   if (!text)
      return INVALID_ID;

   editor->text = realloc(editor->text, sizeof(struct Text *) * ((u32) editor->textCount + 1));
   if (!editor->text)
      return INVALID_ID;

   editor->text[editor->textCount] = text;
   return (i32) editor->textCount++;
}

/* warn: todo: add cleanup at some later stage when it works */
/* returns a buffer id.. todo: write nicely later, let's first make it work */
i32 openFile(const char *path)
{
   struct Buffer *buf = NULL;
   i32 textId;

   if (!E.initialized)
      goto failure;
   if ((textId = loadTextFile(&E, path)) == -1)
      goto failure;
   if (!(buf = calloc(1, sizeof(struct Buffer))))
      goto failure;

   *buf = (struct Buffer) {
      .winId            = -1,
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

   /* todo: move these to editorInit */
   fontManagerInit(E.fontFilePath);
   lineShaderInit(E.lineShader);

   struct Text *text = E.text[textId];
   u32 lineCount     = textGetLineCount(text);

   /* todo: allocate these just for the visible lines, not for all the lines. */
   if (!(buf->visLineRenderers = calloc(lineCount, sizeof(i32))))
      goto failure;

   memset(buf->visLineRenderers, -1, sizeof(i32) * lineCount);

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
   if (opts.textId == -1 /*  && !opts.isVirtual */)
      return INVALID_ID;

   struct LineRenderer *renderer = calloc(1, sizeof(struct LineRenderer));
   if (!renderer)
      return INVALID_ID;

   lineRendererInit(renderer, editor->lineShader);

   /* todo: hide strlen behind the text api so that we can later replace it with something more efficient. */
   struct Text *text = editor->text[opts.textId];
   char *lineBytes   = textGetUTF8Line(text, opts.lineIdx);
   u64 lineByteLen   = strlen(lineBytes);
   fontManagerLayoutLine(renderer, lineBytes, lineByteLen);

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
