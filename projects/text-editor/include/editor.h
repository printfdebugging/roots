#ifndef EDITOR_H
#define EDITOR_H

#include "text.h"

#include "font-manager/font-manager.h"
#include "unicode/unicode.h"
#include "utils/containers.h"

#include "GLFW/glfw3.h"
#include "cglm/struct.h"

#include <stdint.h>
#include <assert.h>

#define DEFAULT_FONT_FILE_PATH ASSETS_DIR "fonts/LilexNerdFont-Regular.ttf"
#define DEFAULT_FONT_SIZE 34
#define DEFAULT_WINDOW_ICON ASSETS_DIR "images/icon.png"
#define TABSTOP 3

#define INVALID_ID -1

#define NUL 0x00
#define SPACE 0x20
#define NEWLINE 0x0a
#define HORIZONTAL_TAB 0x09
#define CARRIAGE_RETURN 0x0d

/**!
 * Opaque `Text` type. There would be a few implementations in the backend,
 * a `GapBuffer` implementation, a `Rope` implementation, and the user would
 * be able to choose which implementation they want to use.
 */
struct text;

/**!
 * @brief Usually it's said that buffers are shared between frames, but the
 * sharing is in terms of attributes, like two buffers might share a frame,
 * or two buffers might not share a frame but share the text they are showing.
 *
 * This does not mean that they are the same buffers. They can have different
 * cursor positions, different state that they are rendering, different visibility.
 * So they are not at all the same thing. Don't tell the user about this ;).
 */

/*
 * struct Buffer
 * {
 *    int32_t winId;
 *    int32_t txtId;
 * };
 */

/* this is layout's job not vislinerenderer's
 * so first step is to split line renderers from layouting :) again*/
//};

/**!
 * Font manager is a subsystem we request for the font objects.
 * This way, we don't have to manage the lifetime of these objects. And
 * since these objects are shared, so is the glyphCache.
 */

/* layout space constants */
#define CHARS 60
#define LINES 20
#define VERTICES 6
#define VERTEX_SIZE sizeof(struct glyph_vertex)

#ifdef LOGGING
#define LOG_INFO(...) fprintf(stderr, __VA_ARGS__);
#define LOG_EVENT(...) fprintf(stderr, __VA_ARGS__);
#else
#define LOG_INFO(...)
#define LOG_EVENT(...)
#endif

struct text_layout_map {
	uint32_t text_line_index;
	bool layouted;
	bool uploaded;
};

struct editor {
	/* arrays */
	struct text *text;
	struct GLFWwindow *window;
	struct buffer_renderer *buffer_renderer;
	struct text_shader *text_shader; /* shared among Buffer objects */

	struct glyph_vertex *vertices;
	/* this is fixed by the constants above */
	uint32_t verticesCount;
	struct text_layout_map layout_map[LINES];

	/*
	 * This is a baton which the update -> layout -> upload stages
	 * pass to each other to signal if things changed and whether
	 * they need to do something about it.
	 *
	 * The idea is that each stage just checks "do i need to do anything"
	 * and only when the answer is "yes", should they go out and look
	 * into the layoutMap about what changed.
	 *
	 * - todo: add LayoutType - CURSOR_MOVE, SCROLL, RESIZE..
	 * - todo: add UploadType - MORPH, BUFFER_SUBDATA
	 */
	struct
	{
		bool updated;
		bool layouted;
		bool uploaded;
	} layout_map_state;

	/*
	 * The cursor does not exist for the text, it's just a marker the
	 * user has (in the buffer) to say "make edits here" etc.
	 */
	uint32_t cursor_line;
	uint32_t cursor_column;

	uint32_t line_offset;
	uint32_t column_offset;

	/* config */
	float font_size;
	char *font_file_path;
	bool initialized;

	/* frame book-keeping */
	double time_last;
	double time_delta;
};

struct text_shader_uniforms {
	mat4s mvp;
	ivec4s viewport;
	float scale;
	int32_t hb_gpu_atlas;
	float gamma;
	bool debug;
	bool stem_darkening;
};

struct text_shader_uniform_locations {
	int32_t mvp;
	int32_t viewport;
	int32_t scale;
	int32_t position;
	int32_t hb_gpu_atlas;
	int32_t gamma;
	int32_t foreground;
	int32_t debug;
	int32_t stem_darkening;
};

/**!
 * A Line shader is shared between various line renderers. This
 * does not contain any state, but allows one to quickly set
 * the state using `TextShaderUniforms` and draw/redraw a line..
 */
struct text_shader {
	uint32_t hb_shader_program;
	struct text_shader_uniform_locations uniform_locations;
};

/* for now BufferRender and LineLayout don't know about each other, that's fine. */
struct buffer_renderer {
	/**!
	 * Uniforms of the line, like the position from where we start
	 * drawing, the MVP matrix, the scale, gpu atlas, so on..
	 */
	struct text_shader_uniforms uniforms;

	/* OpenGL primitives */
	uint32_t vao;
	uint32_t vbo;

	/* note: this is inconsiquencial in layouting, considering that we
	 * are going for a fixed buffer approach for now */
	uint32_t count;
	bool uploaded;
};

enum editor_update_event {
	EDITOR_STARTUP,
	KEY_PRESS,
};

union editor_update_state {
	int32_t glfw_key;
};

/**!
 * These are the core editor functions, so they can access the editor
 * directly.
 */
bool editor_init();
void editor_calculate_frame_time();
bool editor_run();
bool editor_should_close();
bool editor_deinit();
void editor_update(enum editor_update_event event, union editor_update_state state);
void editor_layout();
void editor_upload_to_gpu();
void editor_render();

void buffer_layout();
void buffer_upload_to_gpu();
void buffer_renderer_init(struct buffer_renderer *renderer, struct text_shader *shader);
void buffer_renderer_deinit(struct buffer_renderer *renderer);
void buffer_render();

bool file_open(const char *filePath); /* note: remove me */
void EditorOpenFile(const char *path);

void text_shader_create(struct text_shader *shader);
void text_shader_destroy(struct text_shader *shader);
void text_shader_upload_uniforms(struct text_shader *shader, struct text_shader_uniforms *uniforms);

#endif
