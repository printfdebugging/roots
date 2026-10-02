#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "hb.h"
#include "hb-gpu.h"
#include "hb-ot.h"
#include "cglm/struct.h"

#include <stdint.h>

/* todo: document it properly */
/* todo: also pass the text offsets, so it's easy to map clicks to cursor position changes */
struct glyph_vertex {
	float x;
	float y;
	float tx;
	float ty;
	float nx;
	float ny;
	float epp;
	uint32_t atlas_offset;
	uint32_t has_cursor;

	/*
	todo:
		vec4 textPos;
			[0] = textId
			[1] = line
			[2] = byteOffset
	*/

	vec4s fg_color;
	vec4s bg_color;
};

struct extents {
	double min_x;
	double min_y;
	double max_x;
	double max_y;
};

struct glyph_atlas {
	uint32_t texture;
	int32_t texture_unit;
	uint32_t texture_buffer_object;
	uint32_t capacity_bytes;

	/**!
	 * note: This doesn't start at 0, but at `TEXEL_SIZE`. Empty glyphs
	 * don't have any glyph data, so their `GlyphInfo.atlasOffset` is set
	 * to 0, and if we start at 0 here, that would then use the first uploaded
	 * glyph for the spaces..
	 */
	uint32_t cursor_offset_bytes;
};

struct glyph_info {
	double advance;
	int32_t upem;
	uint32_t atlas_offset;
	bool empty;
	bool cached;
	struct extents extents;
};

struct font {
	char *font_path;

	/* font objects & the encoder */
	hb_face_t *hb_face;
	hb_font_t *hb_font;
	hb_gpu_draw_t *hb_draw;

	/* font metrics */
	int32_t hb_ascent;
	int32_t hb_descent;
	int32_t hb_max_height;

	struct glyph_info *glyph_cache;
};

struct font_manager {
	struct font *font;
	uint32_t font_count;

	/**!
	 * Path the default editor font.
	 */
	const char *editor_font_path;

	/**!
	 * The default font of the editor. Every rune is first shaped
	 * with this font and if it doesn't have a glyph, we check other
	 * cached fonts then the system fonts using fontconfig.
	 */
	struct font *editor_font;

	/**!
	 * OpenGL textures with the glyph data. `GlyphInfo.atlasOffset` is an
	 * offset into this texture. We only cache the glyphs being used, so
	 * even if we are using a few fonts, it should be fine for the most part.
	 */
	struct glyph_atlas atlas;

	bool initialized;
};

/* fontmanager.c */
/* note: todo: this should take font_manager_options */
void font_manager_init(char *editor_font_path);
void font_manager_deinit();
void font_manager_cache_glyph_info(struct font *font, uint32_t glyphidx);
struct glyph_atlas *font_manager_get_atlas();
struct font *font_manager_get_font(const char *file_path);
struct font *font_manager_get_default_font();

float font_manager_get_default_font_scale();
float font_manager_get_default_font_line_height();

struct font *font_manager_get_font_with_rune(uint32_t codepoint);

void font_init(struct font *font, const char *filepath);
void font_deinit(struct font *font);

void _font_manager_atlas_init();
void _font_manager_atlas_deinit();

// void _linePrintChars();
// void _lineSubstituteNewlines();
// void _lineSubstituteTabs();

#endif
