#ifndef PLATFORM_WINDOW_H
#define PLATFORM_WINDOW_H

#include "glad/glad.h"
#include "GLFW/glfw3.h"

/*
 * This is the userdata struct which the client defines & passes a pointer to
 * on the GLFWwindow. With this, we can simply avoid having to create a wrapper
 * around GLFWwindow, because GLFWwindow is the appropriate level of abstraction
 * for a window object.
 */
struct window_userdata;

struct window_options {
	bool visible;
	bool transparent;
	int32_t width;
	int32_t height;
	const char *title;
	const char *icon;
	GLFWwindow *shared_context_window;

	GLFWframebuffersizefun framebuffer_resize_callback;
	GLFWscrollfun scroll_callback;
	GLFWcursorposfun cursor_position_callback;
	GLFWkeyfun key_callback;
	GLFWerrorfun error_callback;
};

GLFWwindow *window_create(struct window_options opts);

// void window_destroy(struct GLFWwindow *window);

struct rectangle window_get_bounds(struct GLFWwindow *window);

void window_swap_buffers(struct GLFWwindow *window);

/*
 * These are just declarations. They should be defined by the
 * user of this module. That simplifies things a lot, otherwise we would
 * have to declare some kind of interface here which would redirect
 * callbacks, and the last time that was attempted, it didn't go that well.
 */
void window_scroll_callback(GLFWwindow *window, double x, double y);
void window_frame_buffer_resize_callback(GLFWwindow *window, int32_t width, int32_t height);
void window_cursor_position_callback(GLFWwindow *window, double x, double y);
void window_key_callback(GLFWwindow *window, int32_t key, int32_t scancode, int32_t action, int32_t mods);
void window_error_callback(int code, const char *description);

#endif
