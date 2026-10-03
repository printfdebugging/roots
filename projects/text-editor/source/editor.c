#include "editor.h"

#include "utils/macros.h"
#include "utils/constants.h"
#include "platform/window.h"

#include "glad/glad.h"
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct editor E = { 0 };

bool editor_run() {
	const char *path = SOURCE_DIR "source/editor.c";

	if (!E.initialized)
		perror("E not initialized\n");
	if (!(E.text = text_load_from_file(path)))
		return false;

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

	struct window_options opts = {
		.width = 800,
		.height = 600,
		.title = "GLFWwindow",
		.transparent = false,
		.visible = true,
		.icon = DEFAULT_WINDOW_ICON,
		.shared_context_window = NULL,
	};

	E.window = window_create(opts);
	if (!E.window)
		perror("failed to create a window");

	if (!(E.shared_text_shader = calloc(1, sizeof(struct text_shader))))
		return false;
	if (!(E.buffer_renderer = calloc(1, sizeof(struct buffer_renderer))))
		return false;

	E.vertices_count = CHARS * LINES * VERTICES;
	if (!(E.vertices = calloc(E.vertices_count, VERTEX_SIZE)))
		return false;

	font_manager_init((struct font_manager_options) {
		.default_font_path = E.font_file_path,
		.default_font_size = (uint32_t) E.font_size,
	});

	text_shader_create(E.shared_text_shader);
	buffer_renderer_init(E.buffer_renderer, E.shared_text_shader);
	glBufferData(GL_ARRAY_BUFFER, E.vertices_count * VERTEX_SIZE, NULL, GL_STATIC_DRAW);

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
	text_shader_destroy(E.shared_text_shader);
	buffer_renderer_deinit(E.buffer_renderer);
	font_manager_deinit();

	text_destroy(E.text);
	free(E.text);

	free(E.shared_text_shader);
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
	if (E.vertices_count == 0)
		return;

	glBindVertexArray(E.buffer_renderer->vao);
	glBindBuffer(GL_ARRAY_BUFFER, E.buffer_renderer->vbo);

	for (uint32_t idx = 0; idx < LINES; ++idx) {
		if (E.layout_map[idx].layouted && !E.layout_map[idx].uploaded) {
			uint32_t offset = CHARS * VERTICES * idx;
			uint32_t byte_offset = offset * VERTEX_SIZE;
			uint32_t count = CHARS * VERTICES * VERTEX_SIZE;
			glBufferSubData(GL_ARRAY_BUFFER, byte_offset, count, E.vertices + offset);
			E.layout_map[idx].uploaded = true;
		}
	}

	E.buffer_renderer->uploaded = true;
}

/*
 * This should prepare the layout_map such that
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

			for (uint32_t line_index = 0; line_index < LINES; ++line_index) {
				E.layout_map[line_index] = (struct text_layout_map) {
					.text_line_index = line_index,
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

					bool on_last_line = E.cursor_line == line_count - 1;
					if (on_last_line)
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
					bool on_first_line = E.cursor_line == 0;
					if (on_first_line)
						break;

					E.cursor_line -= 1;

					/* dup */
					uint32_t line_length = text_get_line_length(E.text, E.cursor_line);
					if (line_length < E.cursor_column)
						E.cursor_column = line_length - 1;

					break;
				}
				case GLFW_KEY_LEFT: {
					LOG_EVENT("update: GLFW_KEY_LEFT\n");

					bool on_first_column = E.cursor_column == 0;
					if (on_first_column)
						break;

					E.cursor_column -= 1;

					break;
				}
				case GLFW_KEY_RIGHT: {
					LOG_EVENT("update: GLFW_KEY_RIGHT\n");

					uint32_t line_length = text_get_line_length(E.text, E.cursor_line);
					if (E.cursor_column < line_length - 1)
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
			LOG_INFO("E.line_offset (%i) + LINES (%i) <= E.cursor_line (%i)\n", E.line_offset, LINES, E.cursor_line);
			for (uint32_t lineidx = 0; lineidx < LINES; ++lineidx) {
				/* todo: note: this should be shifting rather than just blind increment */
				E.layout_map[lineidx].layouted = false;
				E.layout_map[lineidx].text_line_index += 1;
			}
			E.line_offset++;
		} else if (E.cursor_line < E.line_offset) {
			LOG_INFO("E.cursor_line (%i) < E.line_offset (%i)\n", E.cursor_line, E.line_offset);
			for (uint32_t lineidx = 0; lineidx < LINES; ++lineidx) {
				/* todo: note: this should be shifting rather than just blind increment */
				E.layout_map[lineidx].layouted = false;
				E.layout_map[lineidx].text_line_index -= 1;
			}
			E.line_offset--;
		} else {
			LOG_INFO("E.layout_map[E.cursor_line (%i) - E.line_offset (%i)].layouted (%i) = false;\n", E.cursor_line, E.line_offset, E.layout_map[E.cursor_line - E.line_offset].layouted);
			E.layout_map[old_cursor_line - E.line_offset].layouted = false;
			E.layout_map[E.cursor_line - E.line_offset].layouted = false;
		}

		E.layout_map_state.updated = true;
		E.layout_map_state.layouted = false;

		/* if scroll past the edges, then all lines relayout. middle ones just move one step up */
		/* if scroll within visible range, invalidate both lines. later with a cursor moved flag */
	}

	if (E.cursor_column != old_cursor_column) {
		LOG_INFO("E.cursor_column (%i) != old_cursor_column (%i)\n", E.cursor_column, old_cursor_column);
		if (E.column_offset + CHARS <= E.cursor_column) {
			/* cursor move right */
			LOG_INFO("E.column_offset (%i) + CHARS (%i) < E.cursor_column (%i)\n", E.column_offset, CHARS, E.cursor_column)
			for (uint32_t lineidx = 0; lineidx < LINES; ++lineidx)
				E.layout_map[lineidx].layouted = false;

			uint32_t last_visible_column_index = E.column_offset + CHARS - 1;
			E.column_offset += (E.cursor_column - last_visible_column_index);
		} else if (E.cursor_column < E.column_offset) {
			/* cursor move left */
			LOG_INFO("E.cursor_column (%i) < E.column_offset (%i)\n", E.cursor_column, E.column_offset)
			for (uint32_t lineidx = 0; lineidx < LINES; ++lineidx)
				E.layout_map[lineidx].layouted = false;
			E.column_offset -= (E.column_offset - E.cursor_column);
		} else {
			/* moved over visible columns */
			LOG_INFO("E.layout_map[E.cursor_line(%i) - E.line_offset (%i)].layouted (%i) = false;\n", E.cursor_line, E.line_offset, E.layout_map[E.cursor_line - E.line_offset].layouted);
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

/* todo: remove editor from here */
void buffer_render() {
	struct glyph_atlas *atlas = font_manager_get_atlas();
	struct rectangle bounds = window_get_bounds(E.window);

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

	text_shader_upload_uniforms(E.shared_text_shader, &E.buffer_renderer->uniforms);

	if (E.buffer_renderer->uploaded) {
		glBindVertexArray(E.buffer_renderer->vao);
		glDrawArrays(GL_TRIANGLES, 0, (int32_t) E.vertices_count);
	}

	/* note: not sure if this should be done after each buffer is rendered, or after all of them
	 * are rendered. for now we just do it here since we only have a single buffer. */
	window_swap_buffers(E.window);
}

struct glyph_info *_glyph_info = NULL;

void buffer_layout() {
	if (!E.shared_text_shader || !E.text || !E.window)
		return;

	float line_height = font_manager_get_default_font_line_height();
	float font_scale = font_manager_get_default_font_scale();

	struct rectangle bounds = window_get_bounds(E.window);
	for (uint32_t visual_line_index = 0; visual_line_index < LINES; ++visual_line_index) {
		if (E.layout_map[visual_line_index].layouted)
			continue;

		/**!
		 * scale = #pixels one point represents
		 * points * scale = pixels
		 * pixels / scale = points
		 */
		vec2s line_position = {
			.x = 0,
			.y = ((float) bounds.h - ((float) (visual_line_index + 1) * line_height)) / font_scale,
		};

		struct font *font = font_manager_get_default_font();
		hb_buffer_t *buffer = hb_buffer_create();

		struct string_view view = text_get_line_utf8_at_offset(E.text, E.layout_map[visual_line_index].text_line_index, E.column_offset);
		hb_buffer_add_utf8(buffer, view.data, (int32_t) view.count, 0, -1);
		hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
		hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
		hb_shape(font->hb_font, buffer, NULL, 0);

		uint32_t hb_glyph_count = 0;
		hb_glyph_info_t *hb_glyph_info = hb_buffer_get_glyph_infos(buffer, &hb_glyph_count);

		_glyph_info = realloc(_glyph_info, hb_glyph_count * sizeof(struct glyph_info));
		if (_glyph_info)
			memset(_glyph_info, 0, hb_glyph_count * sizeof(struct glyph_info));

		uint32_t glyph_count = hb_glyph_count < CHARS ? hb_glyph_count : CHARS;

		for (uint32_t idx = 0; idx < glyph_count; ++idx) {
			hb_codepoint_t glyph_index = hb_glyph_info[idx].codepoint;
			font_manager_cache_glyph_info(font, glyph_index);
			_glyph_info[idx] = font->glyph_cache[glyph_index];
		}

		hb_buffer_destroy(buffer);

		struct point glyph_position = {
			.x = line_position.x,
			.y = line_position.y,
		};

		/* we loop over the available slots */
		for (uint32_t idx = 0; idx < glyph_count; ++idx) {
			struct glyph_info *glyph_info = &_glyph_info[idx];
			glyph_position.x += glyph_info->extents.min_x;
			glyph_position.y += 0;

			struct glyph_vertex glyph_quad_corners[4];

			uint32_t visual_column = 0;
			if (E.cursor_column - E.column_offset >= CHARS)
				visual_column = CHARS - 1;
			else if (E.cursor_column < E.column_offset)
				visual_column = 0;
			else
				visual_column = E.cursor_column - E.column_offset;

			LOG_INFO(
				 "hasCursor: %i, E.cursor_line (%i) == E.layout_map[visual_line_index].text_line_index (%i) && visualColumn (%i) == idx (%i)\n",
				 (E.cursor_line == E.layout_map[visual_line_index].text_line_index && visual_column == idx),
				 E.cursor_line,
				 E.layout_map[visual_line_index].text_line_index,
				 visual_column,
				 idx
			)

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
					.has_cursor = (E.cursor_line == E.layout_map[visual_line_index].text_line_index && visual_column == idx),
				};
			}

			uint32_t glyph_offset = idx * VERTICES;
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

void window_error_callback(int code, const char *description) {
	fprintf(stderr, "window_error_callback: code: %i, msg: %s\n", code, description);
}

void window_framebuffer_resize_callback(GLFWwindow *window, int32_t width, int32_t height) {
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

	bool quit_sequence_pressed = (mods & GLFW_MOD_SHIFT) && (key == GLFW_KEY_Q) && (action == GLFW_PRESS);
	if (quit_sequence_pressed)
		glfwSetWindowShouldClose(window, GLFW_TRUE);

	if (action == GLFW_PRESS || action == GLFW_REPEAT)
		editor_update(KEY_PRESS, (union editor_update_state) { .glfw_key = key });
}
