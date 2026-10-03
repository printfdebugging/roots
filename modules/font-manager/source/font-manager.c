#include "font-manager/font-manager.h"

#include "utils/string.h"
#include "utils/constants.h"

#include "glad/glad.h"

#include <string.h>

static struct font_manager fm = { 0 };

/**!
 * note: FontManager is just a wrapper around harfbuzz & OpenGL functions,
 * and manages shared objects.. So the OpenGL function pointers should be
 * loaded before this function is called. That's done by GLFW.
 */
void font_manager_init(struct font_manager_options opts) {
	if (fm.initialized)
		return;

	/* `fontManagerGetFont` checks this and returns early if false (default) */
	fm.initialized = true;

	_font_manager_atlas_init();
	fm.default_font_path = string_duplicate(opts.default_font_path);
	fm.editor_font = font_manager_get_font(opts.default_font_path);
}

void font_manager_deinit() {
	if (!fm.initialized)
		return;

	for (uint32_t idx = 0; idx < fm.font_count; ++idx)
		font_deinit(&fm.font[idx]);

	free(fm.font);
	free((void *) fm.default_font_path);

	_font_manager_atlas_deinit();
	fm.initialized = false;
}

void font_manager_cache_glyph_info(struct font *font, uint32_t glyphidx) {
	if (font->glyph_cache[glyphidx].cached)
		return;

	struct glyph_info *glyph = &font->glyph_cache[glyphidx];

	int32_t xScale, yScale;
	hb_font_get_scale(font->hb_font, &xScale, &yScale);
	hb_gpu_draw_clear(font->hb_draw);
	hb_gpu_draw_glyph(font->hb_draw, font->hb_font, glyphidx);

	hb_glyph_extents_t hbGlyphExtents = {};
	hb_blob_t *hbBlob = NULL;

	hbBlob = hb_gpu_draw_encode(font->hb_draw, &hbGlyphExtents);
	uint32_t hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

	*glyph = (struct glyph_info) {
		.extents.min_x = 0,
		.extents.max_x = hb_font_get_glyph_h_advance(font->hb_font, glyphidx),
		.extents.min_y = font->hb_descent,
		.extents.max_y = font->hb_ascent,
		.advance = hb_font_get_glyph_h_advance(font->hb_font, glyphidx),
		.upem = yScale,
		.empty = (hbBlobLength == 0),
		.cached = true,
	};

	/* upload glyph data to glyph atlas */
	struct glyph_atlas *glyphAtlas = font_manager_get_atlas();
	if (!glyph->empty) {
		const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
		glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->texture_buffer_object);
		glBufferSubData(GL_TEXTURE_BUFFER, glyphAtlas->cursor_offset_bytes, hbBlobLength, hbGlyphData);
		glyph->atlas_offset = glyphAtlas->cursor_offset_bytes;
		glyphAtlas->cursor_offset_bytes += hbBlobLength;

		hb_gpu_draw_recycle_blob(font->hb_draw, hbBlob);
	}
}

struct glyph_atlas *font_manager_get_atlas() {
	if (!fm.initialized)
		return NULL;
	return &fm.atlas;
}

struct font *font_manager_get_font(const char *file_path) {
	if (!fm.initialized)
		return NULL;

	for (uint32_t idx = 0; idx < fm.font_count; ++idx)
		if (strcmp(fm.font[idx].font_path, file_path) == 0)
			return &fm.font[idx];

	fm.font = realloc(fm.font, sizeof(struct font) * (fm.font_count + 1));
	struct font *font = &fm.font[fm.font_count++];
	font_init(font, file_path);

	return font;
}

struct font *font_manager_get_default_font() {
	if (!fm.initialized)
		return NULL;
	return fm.editor_font;
}

float font_manager_get_default_font_scale() {
	if (!fm.initialized)
		return 0;

	int32_t xScale, yScale;
	hb_font_get_scale(fm.editor_font->hb_font, &xScale, &yScale);
	/* note: todo: temporarily setting this to this default value */
	return 30 / (float) yScale;
}

float font_manager_get_default_font_line_height() {
	if (!fm.initialized)
		return 0;

	float line_height = (float) fm.editor_font->hb_ascent - (float) fm.editor_font->hb_descent;
	return line_height * font_manager_get_default_font_scale();
}

struct font *font_manager_get_font_with_rune(uint32_t codepoint) {
	(void) codepoint;
	perror("todo");
	return NULL;
}

void font_init(struct font *font, const char *filepath) {
	font->font_path = string_duplicate(filepath);
	font->glyph_cache = calloc(U16_MAX, sizeof(struct glyph_info));

	hb_blob_t *hbBlob = NULL;
	if (!(hbBlob = hb_blob_create_from_file(font->font_path)) ||
		 !(font->hb_face = hb_face_create(hbBlob, 0)) ||
		 !(font->hb_font = hb_font_create(font->hb_face)) ||
		 !(font->hb_draw = hb_gpu_draw_create_or_fail())) {
		perror("failed to initialize harfbuzz");
	}

	hb_blob_destroy(hbBlob);

	const hb_ot_metrics_tag_t ASCENT_HHEA = HB_TAG('H', 'a', 's', 'c');
	const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

	hb_ot_metrics_get_position(font->hb_font, ASCENT_HHEA, &font->hb_ascent);
	hb_ot_metrics_get_position(font->hb_font, DESCENT_HHEA, &font->hb_descent);
	hb_ot_metrics_get_position(font->hb_font, HB_OT_METRICS_TAG_CAP_HEIGHT, &font->hb_max_height);
}

void font_deinit(struct font *font) {
	hb_font_destroy(font->hb_font);
	hb_face_destroy(font->hb_face);
	hb_gpu_draw_destroy(font->hb_draw);

	free(font->font_path);
	free(font->glyph_cache);
}

void _font_manager_atlas_init() {
	struct glyph_atlas *atlas = &fm.atlas;

	atlas->capacity_bytes = ATLAS_PAGE_SIZE;
	atlas->cursor_offset_bytes = TEXEL_SIZE;
	atlas->texture_unit = 0;
	glGenBuffers(1, &atlas->texture_buffer_object);
	glBindBuffer(GL_TEXTURE_BUFFER, atlas->texture_buffer_object);
	glBufferData(GL_TEXTURE_BUFFER, atlas->capacity_bytes, NULL, GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE0 + (uint32_t) atlas->texture_unit);
	glGenTextures(1, &atlas->texture);
	glBindTexture(GL_TEXTURE_BUFFER, atlas->texture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, atlas->texture_buffer_object);
}

void _font_manager_atlas_deinit() {
	struct glyph_atlas *glyphAtlas = &fm.atlas;
	glDeleteBuffers(1, &glyphAtlas->texture_buffer_object);
	glDeleteTextures(1, &glyphAtlas->texture);
}
