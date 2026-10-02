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

static struct editor E = { 0 };

void _glfwErrFn(int code, const char *description);

#ifdef _WIN32
static bool _msIsDarkMode();
#endif

bool editor_run() {
	const char *path = SOURCE_DIR "source/editor.c";

	if (!E.initialized)
		perror("E not initialized\n");
	if (!file_open(path))
		perror("Failed to load Text file\n");

	editor_update(EDITOR_STARTUP, (union editor_update_state) {});

	while (!editor_should_close()) {
		editor_calculate_frame_time();
		glfwPollEvents();

		editor_layout();
		editor_upload_to_gpu();
		editor_render();
	}

	return true;
}

bool editor_should_close() {
	if (!E.initialized)
		return true;

	bool should_close = true;
	should_close &= glfwWindowShouldClose(E.window);

	return should_close;
}

bool editor_init() {
	if (E.initialized)
		return true;

	E.font_size = DEFAULT_FONT_SIZE;
	if (!(E.font_file_path = string_duplicate(DEFAULT_FONT_FILE_PATH)))
		return false;

	bool window_exists = window_create(
		 (struct window_options) {
			 .width = 800,
			 .height = 600,
			 .title = "GLFWwindow",
			 .transparent = false,
			 .visible = true,
			 .icon = DEFAULT_WINDOW_ICON,
			 .sharedWinId = INVALID_ID,
			 .framebuffer_resize_callback = window_frame_buffer_resize_callback,
			 .key_callback = window_key_callback,
			 .scroll_callback = window_scroll_callback,
			 .cursor_position_callback = window_cursor_position_callback,
		 }
	);

	if (!window_exists)
		perror("failed to create a window");

	glfwSetErrorCallback(_glfwErrFn);

	if (!(E.text_shader = calloc(1, sizeof(struct text_shader))))
		return false;
	if (!(E.buffer_renderer = calloc(1, sizeof(struct buffer_renderer))))
		return false;

	E.verticesCount = CHARS * LINES * VERTICES;
	if (!(E.vertices = calloc(E.verticesCount, VERTEX_SIZE)))
		return false;

	font_manager_init(E.font_file_path);
	text_shader_create(E.text_shader);
	buffer_renderer_init(E.buffer_renderer, E.text_shader);
	glBufferData(GL_ARRAY_BUFFER, E.verticesCount * VERTEX_SIZE, NULL, GL_STATIC_DRAW);

	for (uint32_t idx = 0; idx < LINES; ++idx)
		E.layout_map[idx].text_line_index = idx;

	E.initialized = true;
	return true;
}

void editor_calculate_frame_time() {
	double time_now = glfwGetTime();
	E.time_delta = time_now - E.time_last;
	E.time_last = time_now;
}

bool editor_deinit() {
	text_shader_destroy(E.text_shader);
	buffer_renderer_deinit(E.buffer_renderer);
	font_manager_deinit();

	text_destroy(E.text);
	free(E.text);

	free(E.text_shader);
	free(E.font_file_path);
	free(E.vertices);

	glfwDestroyWindow(E.window);
	glfwTerminate();

	return true;
}

void editor_render() {
	buffer_render();
}

void editor_upload_to_gpu() {
	if (!E.layout_map_state.uploaded) {
		buffer_upload_to_gpu();
		E.layout_map_state.uploaded = true;
	}
}

void buffer_upload_to_gpu() {
	if (E.verticesCount == 0)
		return;

	glBindVertexArray(E.buffer_renderer->vao);
	glBindBuffer(GL_ARRAY_BUFFER, E.buffer_renderer->vbo);

	for (uint32_t idx = 0; idx < LINES; ++idx) {
		if (E.layout_map[idx].layouted && !E.layout_map[idx].uploaded) {
			uint32_t offset = CHARS * VERTICES * idx;
			uint32_t byteOffset = offset * VERTEX_SIZE;
			uint32_t count = CHARS * VERTICES * VERTEX_SIZE;
			glBufferSubData(GL_ARRAY_BUFFER, byteOffset, count, E.vertices + offset);
			E.layout_map[idx].uploaded = true;
		}
	}

	E.buffer_renderer->uploaded = true;
}

/*
 * This should prepare the layoutMap such that
 * layout can just be sure that that's up to date and just loop over
 * it and either relayout or skip.
 *
 * This should be triggered by events like cursor moved, or window resized, etc etc..
 * This should probably take a "who calls the update and for what" param
 */
void editor_update(enum editor_update_event event, union editor_update_state state) {
	uint32_t line_count = text_get_line_count(E.text);
	uint32_t old_cursor_line = E.cursor_line;
	uint32_t old_cursor_column = E.cursor_column;

	/* todo: later: this has update + layouting which is not the right shape to hold.
	 * something has to be done, cursor position has to be updated for sure before the
	 * second half of this function runs which is about layouting. */
	switch (event) {
		/* layouting */
		case EDITOR_STARTUP: {
			E.cursor_column = 0;
			E.cursor_line = 0;

			for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx) {
				E.layout_map[lineIdx] = (struct text_layout_map) {
					.text_line_index = lineIdx,
					.layouted = false,
					.uploaded = false,
				};
			}

			break;
		}
		case KEY_PRESS: {
			/* later: we don't consider the count etc and we should do that. we should
			 * also make sure that we are marking something as "needs cleanup for the
			 * rest of the empty quads" */
			switch (state.glfw_key) {
				/* this crashes the application when key is clicked */
				case GLFW_KEY_DOWN: {
					LOG_EVENT("update: GLFW_KEY_DOWN\n");

					bool alreadyOnTheLastLine = E.cursor_line == line_count - 1;
					if (alreadyOnTheLastLine)
						break;

					E.cursor_line += 1;

					/* dup */
					uint32_t line_length = text_get_line_length(E.text, E.cursor_line);
					if (line_length < E.cursor_column)
						E.cursor_column = line_length - 1;

					break;
				}
				case GLFW_KEY_UP: {
					LOG_EVENT("update: GLFW_KEY_UP\n");

					/* already on the first line */
					bool alreadyOnTheFirstLine = E.cursor_line == 0;
					if (alreadyOnTheFirstLine)
						break;

					E.cursor_line -= 1;

					/* dup */
					uint32_t lineLen = text_get_line_length(E.text, E.cursor_line);
					if (lineLen < E.cursor_column)
						E.cursor_column = lineLen - 1;

					break;
				}
				case GLFW_KEY_LEFT: {
					LOG_EVENT("update: GLFW_KEY_LEFT\n");

					bool alreadyOnTheFirstColumn = E.cursor_column == 0;
					if (alreadyOnTheFirstColumn)
						break;

					E.cursor_column -= 1;

					break;
				}
				case GLFW_KEY_RIGHT: {
					LOG_EVENT("update: GLFW_KEY_RIGHT\n");

					uint32_t lineLen = text_get_line_length(E.text, E.cursor_line);
					if (E.cursor_column < lineLen - 1)
						E.cursor_column += 1;

					break;
				}
				case GLFW_KEY_HOME: {
					E.cursor_column = 0;
					break;
				}
				case GLFW_KEY_END: {
					uint32_t length = text_get_line_length(E.text, E.cursor_line);
					if (length > 0)
						E.cursor_column = length - 1;
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

	if (E.cursor_line != old_cursor_line) {
		if (E.line_offset + LINES <= E.cursor_line) {
			LOG_INFO("E.lineOffset (%i) + LINES (%i) <= E.cursorLine (%i)\n", E.lineOffset, LINES, E.cursorLine);
			for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx) {
				/* todo: note: this should be shifting rather than just blind increment */
				E.layout_map[lineIdx].layouted = false;
				E.layout_map[lineIdx].text_line_index += 1;
			}
			E.line_offset++;
		} else if (E.cursor_line < E.line_offset) {
			LOG_INFO("E.cursorLine (%i) < E.lineOffset (%i)\n", E.cursorLine, E.lineOffset);
			for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx) {
				/* todo: note: this should be shifting rather than just blind increment */
				E.layout_map[lineIdx].layouted = false;
				E.layout_map[lineIdx].text_line_index -= 1;
			}
			E.line_offset--;
		} else {
			LOG_INFO("E.layoutMap[E.cursorLine (%i) - E.lineOffset (%i)].layouted (%i) = false;\n", E.cursorLine, E.lineOffset, E.layoutMap[E.cursorLine - E.lineOffset].layouted);
			E.layout_map[old_cursor_line - E.line_offset].layouted = false;
			E.layout_map[E.cursor_line - E.line_offset].layouted = false;
		}

		E.layout_map_state.updated = true;
		E.layout_map_state.layouted = false;

		/* if scroll past the edges, then all lines relayout. middle ones just move one step up */
		/* if scroll within visible range, invalidate both lines. later with a cursor moved flag */
	}

	if (E.cursor_column != old_cursor_column) {
		LOG_INFO("E.cursorColumn (%i) != oldCurCol (%i)\n", E.cursorColumn, oldCurCol);
		if (E.column_offset + CHARS <= E.cursor_column) /* cursor move right */
		{
			LOG_INFO("E.columnOffset (%i) + CHARS (%i) < E.cursorColumn (%i)\n", E.columnOffset, CHARS, E.cursorColumn)
			for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
				E.layout_map[lineIdx].layouted = false;

			uint32_t lastVisColIdx = E.column_offset + CHARS - 1;
			E.column_offset += (E.cursor_column - lastVisColIdx);
		} else if (E.cursor_column < E.column_offset) /* cursor move left */
		{
			LOG_INFO("E.cursorColumn (%i) < E.columnOffset (%i)\n", E.cursorColumn, E.columnOffset)
			for (uint32_t lineIdx = 0; lineIdx < LINES; ++lineIdx)
				E.layout_map[lineIdx].layouted = false;
			E.column_offset -= (E.column_offset - E.cursor_column);
		} else /* moved over visible columns */
		{
			LOG_INFO("E.layoutMap[E.cursorLine (%i) - E.lineOffset (%i)].layouted (%i) = false;\n", E.cursorLine, E.lineOffset, E.layoutMap[E.cursorLine - E.lineOffset].layouted);
			E.layout_map[E.cursor_line - E.line_offset].layouted = false;
		}

		E.layout_map_state.updated = true;
		E.layout_map_state.layouted = false;
	}
}

void editor_layout() {
	if (!E.layout_map_state.layouted) {
		buffer_layout();
		E.layout_map_state.layouted = true;
		E.layout_map_state.uploaded = false;
	}
}

/**!
 * Loads the text file from `filePath` into a `Text` object,
 * and returns an index to it, or `INVALID_ID` on error.
 */
bool file_open(const char *filePath) {
	if (!filePath)
		return false;
	if (!(E.text = text_load_from_file(filePath)))
		return false;
	return true;
}

/* warn: todo: add cleanup at some later stage when it works */
/* returns a buffer id.. todo: write nicely later, let's first make it work */
void EditorOpenFile(const char *path) {
	(void) path;
	/* todo: */
}

/* todo: remove editor from here */
void buffer_render() {
	struct glyph_atlas *atlas = font_manager_get_atlas();
	struct rectangle bounds = window_get_bounds();

	mat4s mvp = { GLM_MAT4_IDENTITY_INIT };
	mvp = glms_ortho(0, (float) bounds.w, 0, (float) bounds.h, 0.0f, 100.0f);
	mvp = glms_translate(mvp, (vec3s) { { 0.0f, 0.0f, 0.0f } }); /* not set as of now */

	ivec4s viewport = { 0 };
	glGetIntegerv(GL_VIEWPORT, viewport.raw);

	glClearColor(color_rgba_hex(0X002b36FF));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float font_scale = font_manager_get_default_font_scale();

	E.buffer_renderer->uniforms = (struct text_shader_uniforms) {
		.mvp = mvp,
		.viewport = viewport,
		.scale = font_scale,
		.hb_gpu_atlas = atlas->texture_unit,
		.gamma = 1.0f,
		.debug = false,
		.stem_darkening = false,
	};

	text_shader_upload_uniforms(E.text_shader, &E.buffer_renderer->uniforms);

	if (E.buffer_renderer->uploaded) {
		glBindVertexArray(E.buffer_renderer->vao);
		glDrawArrays(GL_TRIANGLES, 0, (int32_t) E.verticesCount);
	}

	/* note: not sure if this should be done after each buffer is rendered, or after all of them
	 * are rendered. for now we just do it here since we only have a single buffer. */
	swap_buffers();
}

struct glyph_info *_glyphInfo = NULL;

void buffer_layout() {
	if (!E.text_shader || !E.text || !E.window)
		return;

	float lineHeight = font_manager_get_default_font_line_height();
	float fontScale = font_manager_get_default_font_scale();

	struct rectangle bounds = window_get_bounds();
	for (uint32_t visual_line_index = 0; visual_line_index < LINES; ++visual_line_index) {
		if (E.layout_map[visual_line_index].layouted)
			continue;

		/**!
		 * scale = #pixels one point represents
		 * points * scale = pixels
		 * pixels / scale = points
		 */
		vec2s linePos = { .x = 0, .y = ((float) bounds.h - ((float) (visual_line_index + 1) * lineHeight)) / fontScale };

		struct font *font = font_manager_get_default_font();
		hb_buffer_t *buffer = hb_buffer_create();

		struct string_view view = text_get_line_utf8_at_offset(E.text, E.layout_map[visual_line_index].text_line_index, E.column_offset);
		hb_buffer_add_utf8(buffer, view.data, (int32_t) view.count, 0, -1);
		hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
		hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
		hb_shape(font->hb_font, buffer, NULL, 0);

		uint32_t hbGlyphCount = 0;
		hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &hbGlyphCount);

		_glyphInfo = realloc(_glyphInfo, hbGlyphCount * sizeof(struct glyph_info));
		if (_glyphInfo)
			memset(_glyphInfo, 0, hbGlyphCount * sizeof(struct glyph_info));

		uint32_t glyph_count = hbGlyphCount < CHARS ? hbGlyphCount : CHARS;

		for (uint32_t glyphIdx = 0; glyphIdx < glyph_count; ++glyphIdx) {
			hb_codepoint_t glyphIndex = glyphInfos[glyphIdx].codepoint;
			font_manager_cache_glyph_info(font, glyphIndex);
			_glyphInfo[glyphIdx] = font->glyph_cache[glyphIndex];
		}

		hb_buffer_destroy(buffer);

		struct point glyph_position = {
			.x = linePos.x,
			.y = linePos.y,
		};

		/* we loop over the available slots */
		for (uint32_t glyph_idx = 0; glyph_idx < glyph_count; ++glyph_idx) {
			[[maybe_unused]] bool hasCursor;
			struct glyph_info *glyph_info = &_glyphInfo[glyph_idx];

			glyph_position.x += glyph_info->extents.min_x;
			glyph_position.y += 0;

			struct glyph_vertex glyph_quad_corners[4];

			uint32_t visualColumn = 0;
			if (E.cursor_column - E.column_offset >= CHARS)
				visualColumn = CHARS - 1;
			else if (E.cursor_column < E.column_offset)
				visualColumn = 0;
			else
				visualColumn = E.cursor_column - E.column_offset;

			/* LOG_INFO(
				 "hasCursor: %i, E.cursorLine (%i) == E.layoutMap[visLineIdx].textLineIdx (%i) && visualColumn (%i) == glyphIdx (%i)\n",
				 (E.cursorLine == E.layoutMap[visLineIdx].textLineIdx && visualColumn == glyphIdx),
				 E.cursorLine,
				 E.layoutMap[visLineIdx].textLineIdx,
				 visualColumn,
				 glyphIdx
			) */

			uint32_t line_offset = visual_line_index * CHARS * VERTICES;
			for (int corner_idx = 0; corner_idx < 4; corner_idx++) {
				int32_t cx = (corner_idx >> 1) & 1;
				int32_t cy = corner_idx & 1;
				double ex = (1 - cx) * glyph_info->extents.min_x + cx * glyph_info->extents.max_x;
				double ey = (1 - cy) * glyph_info->extents.min_y + cy * glyph_info->extents.max_y;

				glyph_quad_corners[corner_idx] = (struct glyph_vertex) {
					.x = (float) glyph_position.x,
					.y = (float) glyph_position.y,
					.tx = (float) ex,
					.ty = (float) ey,
					.nx = cx ? 1.f : -1.f,
					.ny = cy ? -1.f : 1.f,
					.epp = 1.0,
					.atlas_offset = glyph_info->atlas_offset / TEXEL_SIZE,
					.fg_color = (vec4s) { { color_rgba_hex(0X839496FF) } },
					.bg_color = (vec4s) { { color_rgba_hex(0X000000FF) } },
					.has_cursor = (E.cursor_line == E.layout_map[visual_line_index].text_line_index && visualColumn == glyph_idx),
				};
			}

			uint32_t glyph_offset = glyph_idx * VERTICES;
			uint32_t glyph_quad_offset = line_offset + glyph_offset;
			E.vertices[glyph_quad_offset + 0] = glyph_quad_corners[0];
			E.vertices[glyph_quad_offset + 1] = glyph_quad_corners[1];
			E.vertices[glyph_quad_offset + 2] = glyph_quad_corners[2];
			E.vertices[glyph_quad_offset + 3] = glyph_quad_corners[1];
			E.vertices[glyph_quad_offset + 4] = glyph_quad_corners[2];
			E.vertices[glyph_quad_offset + 5] = glyph_quad_corners[3];

			/* note: todo: this currently assumes the layout to be horizontal, fine assumption
			 * when starting out, but later we would also want to cater for the vertical
			 * writing styles. */
			glyph_position.x += glyph_info->extents.max_x;
		}

		/* this would do for now */
		if (glyph_count < CHARS) {
			uint32_t line_offset = (visual_line_index * CHARS * VERTICES);
			for (uint32_t glyph_idx = glyph_count; glyph_idx < CHARS; ++glyph_idx) {
				uint32_t glyph_offset = glyph_idx * VERTICES;
				uint32_t glyph_quad_offset = line_offset + glyph_offset;

				E.vertices[glyph_quad_offset + 0] = (struct glyph_vertex) { 0 };
				E.vertices[glyph_quad_offset + 1] = (struct glyph_vertex) { 0 };
				E.vertices[glyph_quad_offset + 2] = (struct glyph_vertex) { 0 };
				E.vertices[glyph_quad_offset + 3] = (struct glyph_vertex) { 0 };
				E.vertices[glyph_quad_offset + 4] = (struct glyph_vertex) { 0 };
				E.vertices[glyph_quad_offset + 5] = (struct glyph_vertex) { 0 };
			}
		}

		E.layout_map[visual_line_index].layouted = true;
		E.layout_map[visual_line_index].uploaded = false;
	}
}

bool window_create(struct window_options opts) {
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

	if (!img.pixels) {
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

	if (opts.cursor_position_callback) glfwSetCursorPosCallback(window, opts.cursor_position_callback);
	if (opts.scroll_callback) glfwSetScrollCallback(window, opts.scroll_callback);
	if (opts.framebuffer_resize_callback) glfwSetFramebufferSizeCallback(window, opts.framebuffer_resize_callback);
	if (opts.key_callback) glfwSetKeyCallback(window, opts.key_callback);

	if (!window)
		return false;

	E.window = window;
	return true;
}

struct rectangle window_get_bounds() {
	if (!E.window)
		return (struct rectangle) {};

	int32_t width, height;
	glfwGetWindowSize(E.window, &width, &height);

	return (struct rectangle) {
		.x = 0,
		.y = 0,
		.w = width,
		.h = height,
	};
}

void swap_buffers() {
	glfwSwapBuffers(E.window);
}

void _glfwErrFn(int code, const char *description) {
	fprintf(stderr, "_glfwErrFun: code: %i, msg: %s\n", code, description);
}

#ifdef _WIN32
static bool _msIsDarkMode() {
	HINSTANCE uxThemeLib = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!uxThemeLib) {
		fprintf(stderr, "failed to open uxtheme.dll\n");
		return true; /* default to dark mode */
	}

	bool useDarkMode = GetProcAddress(uxThemeLib, MAKEINTRESOURCEA(132))();
	FreeLibrary(uxThemeLib);
	return useDarkMode;
}
#endif

void window_frame_buffer_resize_callback(GLFWwindow *window, int32_t width, int32_t height) {
	(void) window;
	glViewport(0, 0, width, height);
}

void window_scroll_callback(GLFWwindow *window, double x, double y) {
	(void) window;
	(void) x;
	(void) y;
}

void window_cursor_position_callback(GLFWwindow *window, double x, double y) {
	(void) window;
	(void) x;
	(void) y;
}

void window_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	(void) scancode;
	[[maybe_unused]] struct editor *editor = glfwGetWindowUserPointer(window);

	bool shiftQPress = (mods & GLFW_MOD_SHIFT) && (key == GLFW_KEY_Q) && (action == GLFW_PRESS);
	if (shiftQPress)
		glfwSetWindowShouldClose(window, GLFW_TRUE);

	if (action == GLFW_PRESS || action == GLFW_REPEAT)
		editor_update(KEY_PRESS, (union editor_update_state) { .glfw_key = key });
}
