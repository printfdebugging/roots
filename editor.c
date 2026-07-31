#include <stdio.h>
#include <stdlib.h>

#include "GLFW/glfw3.h"
#include "glad/glad.h"

typedef uint_least8_t u8;
typedef uint_least16_t u16;
typedef uint_least32_t u32;
typedef uint_least64_t u64;
typedef int_least8_t i8;
typedef int_least16_t i16;
typedef int_least32_t i32;
typedef int_least64_t i64;
typedef float f32;
typedef double f64;
typedef bool b8;
typedef int b32;

#define color_rgb_hex(color)                  \
	(((color >> 16) & 0xFF) / 255.0f),    \
	    (((color >> 8) & 0xFF) / 255.0f), \
	    (((color) & 0xFF) / 255.0f)

#define color_rgba_hex(color)              \
	(((color >> 24) & 0xFF) / 255.0f), \
	    color_rgb_hex(color)

GLFWwindow *window = NULL;
f32 lastTime;
f32 timeDelta;

static void mouseScroll(GLFWwindow *window, f64 x, f64 y);
static void windowResize(GLFWwindow *window, i32 width, i32 height);
static void mouseMove(GLFWwindow *window, f64 x, f64 y);

int main(int argc, char *argv[])
{
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
	window = glfwCreateWindow(800, 600, "GLFWWindow", NULL, NULL);
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

	while (!glfwWindowShouldClose(window))
	{
		f64 timeNow = glfwGetTime();
		timeDelta = timeNow - lastTime;
		lastTime = timeNow;

		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);

		glClearColor(color_rgba_hex(0X282C34FF));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glfwSwapBuffers(window);
	}

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
	fprintf(stderr, "mouse scroll\n");
}

void mouseMove(GLFWwindow *window, f64 x, f64 y)
{
	fprintf(stderr, "mouse move\n");
}
