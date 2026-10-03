#include "platform/window.h"
#include "utils/containers.h"

#include "stb_image.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <dwmapi.h>
#include "GLFW/glfw3native.h"
#endif

#ifdef _WIN32
static bool _ms_is_dark_mode();
#endif

GLFWwindow *window_create(struct window_options opts) {
	if (!glfwInit())
		return NULL;

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

	const int32_t width = opts.width ? opts.width : 1600;
	const int32_t height = opts.height ? opts.height : 800;
	const char *title = opts.title ? opts.title : "GLFWwindow";

	GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, opts.shared_context_window);
	if (!window)
		return NULL;

	const int32_t max_width = 2230;
	const int32_t max_height = 1420;
	const int32_t min_width = 800;
	const int32_t min_height = 600;

	glfwSetWindowSizeLimits(window, min_width, min_height, max_width, max_height);
	glfwMakeContextCurrent(window);
	gladLoadGL((GLADloadfunc) glfwGetProcAddress);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSwapInterval(1);

	if (opts.icon) {
#ifndef __APPLE__
		GLFWimage image;
		int channels;
		image.pixels = stbi_load(opts.icon, &image.width, &image.height, &channels, 0);

		if (!image.pixels) {
			glfwDestroyWindow(window);
			return NULL;
		}

		glfwSetWindowIcon(window, 1, &image);
		free(image.pixels);
#endif
	}

#ifdef _WIN32
	HWND hwnd = glfwGetWin32Window(window);
	DWORD value = _ms_is_dark_mode();
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
#endif

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(2);

	glfwSetCursorPosCallback(window, window_cursor_position_callback);
	glfwSetScrollCallback(window, window_scroll_callback);
	glfwSetFramebufferSizeCallback(window, window_framebuffer_resize_callback);
	glfwSetKeyCallback(window, window_key_callback);
	glfwSetErrorCallback(window_error_callback);
	glfwSetWindowUserPointer(window, opts.user_data);

	if (!window)
		return NULL;

	return window;
}

struct rectangle window_get_bounds(GLFWwindow *window) {
	if (!window)
		return (struct rectangle) {};

	int32_t width, height;
	glfwGetWindowSize(window, &width, &height);

	return (struct rectangle) {
		.x = 0,
		.y = 0,
		.w = width,
		.h = height,
	};
}

void window_swap_buffers(GLFWwindow *window) {
	glfwSwapBuffers(window);
}

#ifdef _WIN32
static bool _ms_is_dark_mode() {
	HINSTANCE ux_theme_lib = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!ux_theme_lib) {
		fprintf(stderr, "failed to open uxtheme.dll\n");
		return true; /* default to dark mode */
	}

	bool use_dark_mode = GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(132))();
	FreeLibrary(ux_theme_lib);
	return use_dark_mode;
}
#endif
