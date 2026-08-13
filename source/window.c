#include "glad/glad.h"

#include "editor.h"

/*********************************************************
 * Unlike other init functions, this one creates         *
 * window, so it's deInit would destroy that window.     *
 * This is just a really simple/shallow abstraction,     *
 * not much thought is put into it, so this might change *
 * in some time.                                         *
 ********************************************************/
void windowInit(struct Editor *editor)
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

   editor->window = glfwCreateWindow(1600, 200, "GLFWWindow", NULL, NULL);
   glfwMakeContextCurrent(editor->window);
   gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
   glfwSetInputMode(editor->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
   glfwSwapInterval(1);

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_MULTISAMPLE);
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glLineWidth(2);

   glfwSetCursorPosCallback(editor->window, mouseMove);
   glfwSetScrollCallback(editor->window, mouseScroll);
   glfwSetFramebufferSizeCallback(editor->window, windowResize);
   glfwSetKeyCallback(editor->window, keyPress);
   glfwSetWindowUserPointer(editor->window, editor);
}

void windowDeInit(struct Editor *editor)
{
   glfwDestroyWindow(editor->window);
   glfwTerminate();
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
   struct Editor *editor = glfwGetWindowUserPointer(window);

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
