#include "glad/glad.h"

#include "editor.h"

/*********************************************************
 * Unlike other init functions, this one creates         *
 * window, so it's deInit would destroy that window.     *
 * This is just a really simple/shallow abstraction,     *
 * not much thought is put into it, so this might change *
 * in some time.                                         *
 ********************************************************/

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
   const char *windowTitle = "GLFWWindow";

   GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, NULL);
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

   /**************************
    * update cursor location *
    *************************/
   if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
   {
      if (editor->cursorCol != 0)
         editor->cursorCol -= 1;
   }

   /****************************************************************************************
    * NOTE: instead of incrementing the cursor column, we should ask the editor to do      *
    * it. That way, the editor can progress the cursor internally by multiple bytes        *
    * for non-ASCII characters which take more than one byte in their UTF8 representation, *
    * like "你好世界"                                                                  *
    ***************************************************************************************/

   if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
   {
      /* note: this way of doing things has to go away, since
       * we should use the forthcoming Text api to scroll */
      editor->cursorCol += 1;
      if ((u32) editor->cursorCol >= editor->lineBytelen)
         editor->cursorCol = (i32) editor->lineBytelen - 1;
   }
}
