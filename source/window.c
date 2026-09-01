#include "glad/glad.h"

#include "editor.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <dwmapi.h>
#include "GLFW/glfw3native.h"
#endif

/*********************************************************
 * Unlike other init functions, this one creates         *
 * window, so it's deInit would destroy that window.     *
 * This is just a really simple/shallow abstraction,     *
 * not much thought is put into it, so this might change *
 * in some time.                                         *
 ********************************************************/

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

void windowSetUserDataPtr(GLFWwindow *window, void *userData)
{
   glfwSetWindowUserPointer(window, userData);
}

GLFWwindow *windowCreate()
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

   const i32 windowWidth   = 1600;
   const i32 windowHeight  = 800;
   const char *windowTitle = "GLFWwindow";

   GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, NULL);
   glfwMakeContextCurrent(window);
   gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
   glfwSwapInterval(1);

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

   glfwSetCursorPosCallback(window, mouseMove);
   glfwSetScrollCallback(window, mouseScroll);
   glfwSetFramebufferSizeCallback(window, windowResize);
   glfwSetKeyCallback(window, keyPress);
   return window;
}

void windowDestroy(GLFWwindow *window)
{
   glfwDestroyWindow(window);
   glfwTerminate();
}

void windowResize(GLFWwindow *window, i32 width, i32 height)
{
   (void) window;
   glViewport(0, 0, width, height);
}

void mouseScroll(GLFWwindow *window, f64 x, f64 y)
{
   (void) window;
   (void) x;
   (void) y;
}

void mouseMove(GLFWwindow *window, f64 x, f64 y)
{
   (void) window;
   (void) x;
   (void) y;
}

void keyPress(GLFWwindow *window, int key, int scancode, int action, int mods)
{
   (void) scancode;
   (void) mods;
   struct Editor *editor = glfwGetWindowUserPointer(window);

   bool dirty = false;

   /**!
    * later:
    * There should be a way to connect callbacks to the particular text buffer
    * which triggered those. So either we connect these to the buffer
    * object itself, such that these callbacks have access to the text object
    * like they do at the moment, or more appropriately we have a frame which
    * would get these events and then pass on to the buffer's ??keymap layer??
    * and then that would handle the input ??somehow??
    */
   if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
      dirty |= textMoveCursorLeft(editor->text);

   if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
      dirty |= textMoveCursorRight(editor->text);

   editor->lineDirty = dirty;
}
