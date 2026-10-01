#include "font-manager.h"
#include "utils.h"
#include "defines.h"

#include "glad/glad.h"

#include <string.h>

static struct FontManager fm = { 0 };

/**!
 * note: FontManager is just a wrapper around harfbuzz & OpenGL functions,
 * and manages shared objects.. So the OpenGL function pointers should be
 * loaded before this function is called. That's done by GLFW.
 */
void FontMgrInit(char *editorFontPath)
{
   if (fm.initialized)
      return;

   /* `fontManagerGetFont` checks this and returns early if false (default) */
   fm.initialized = true;

   _fontMgrAtlasInit();
   fm.editorFontPath = StringDuplicate(editorFontPath);
   fm.editorFont = FontMgrGetFont(editorFontPath);
}

void FontMgrDeInit()
{
   if (!fm.initialized)
      return;

   for (u32 fontIdx = 0; fontIdx < fm.fontCount; ++fontIdx)
      FontDeInit(&fm.font[fontIdx]);

   free(fm.font);
   free((void *) fm.editorFontPath);

   _fontMgrAtlasDeInit();
   fm.initialized = false;
}

void FontMgrCacheGlyphInfo(struct Font *font, u32 glyphIndex)
{
   if (font->glyphCache[glyphIndex].cached)
      return;

   struct GlyphInfo *glyph = &font->glyphCache[glyphIndex];

   i32 xScale, yScale;
   hb_font_get_scale(font->hbFont, &xScale, &yScale);
   hb_gpu_draw_clear(font->hbDraw);
   hb_gpu_draw_glyph(font->hbDraw, font->hbFont, glyphIndex);

   hb_glyph_extents_t hbGlyphExtents = {};
   hb_blob_t *hbBlob = NULL;

   hbBlob = hb_gpu_draw_encode(font->hbDraw, &hbGlyphExtents);
   u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

   *glyph = (struct GlyphInfo) {
      .extents.xMin = 0,
      .extents.xMax = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
      .extents.yMin = font->hbDescent,
      .extents.yMax = font->hbAscent,
      .advance = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
      .upem = yScale,
      .empty = (hbBlobLength == 0),
      .cached = true,
   };

   /* upload glyph data to glyph atlas */
   struct GlyphAtlas *glyphAtlas = FontMgrGetAtlas();
   if (!glyph->empty)
   {
      const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
      glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->textureBufferObject);
      glBufferSubData(GL_TEXTURE_BUFFER, glyphAtlas->cursorOffsetBytes, hbBlobLength, hbGlyphData);
      glyph->atlasOffset = glyphAtlas->cursorOffsetBytes;
      glyphAtlas->cursorOffsetBytes += hbBlobLength;

      hb_gpu_draw_recycle_blob(font->hbDraw, hbBlob);
   }
}

struct GlyphAtlas *FontMgrGetAtlas()
{
   if (!fm.initialized)
      return NULL;
   return &fm.glyphAtlas;
}

struct Font *FontMgrGetFont(const char *filePath)
{
   if (!fm.initialized)
      return NULL;

   for (u32 fontIdx = 0; fontIdx < fm.fontCount; ++fontIdx)
      if (strcmp(fm.font[fontIdx].fontPath, filePath) == 0)
         return &fm.font[fontIdx];

   fm.font = realloc(fm.font, sizeof(struct Font) * (fm.fontCount + 1));
   struct Font *font = &fm.font[fm.fontCount++];
   FontInit(font, filePath);

   return font;
}

struct Font *FontMgrGetDefaultFont()
{
   if (!fm.initialized)
      return NULL;
   return fm.editorFont;
}

f32 FontMgrGetDefaultFontScale()
{
   if (!fm.initialized)
      return 0;

   i32 xScale, yScale;
   hb_font_get_scale(fm.editorFont->hbFont, &xScale, &yScale);
   /* note: todo: temporarily setting this to this default value */
   return 30 / (f32) yScale;
}

f32 FontMgrGetDefaultFontLineHeight()
{
   if (!fm.initialized)
      return 0;

   f32 lineHeight = (f32) fm.editorFont->hbAscent - (f32) fm.editorFont->hbDescent;
   return lineHeight * FontMgrGetDefaultFontScale();
}

struct Font *FontMgrGetFontWithRune(rune codepoint)
{
   (void) codepoint;
   perror("todo");
   return NULL;
}

void FontInit(struct Font *font, const char *filePath)
{
   font->fontPath = StringDuplicate(filePath);
   font->glyphCache = calloc(U16_MAX, sizeof(struct GlyphInfo));

   hb_blob_t *hbBlob = NULL;
   if (!(hbBlob = hb_blob_create_from_file(font->fontPath)) ||
       !(font->hbFace = hb_face_create(hbBlob, 0)) ||
       !(font->hbFont = hb_font_create(font->hbFace)) ||
       !(font->hbDraw = hb_gpu_draw_create_or_fail()))
   {
      perror("failed to initialize harfbuzz");
   }

   hb_blob_destroy(hbBlob);

   const hb_ot_metrics_tag_t ASCENT_HHEA = HB_TAG('H', 'a', 's', 'c');
   const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

   hb_ot_metrics_get_position(font->hbFont, ASCENT_HHEA, &font->hbAscent);
   hb_ot_metrics_get_position(font->hbFont, DESCENT_HHEA, &font->hbDescent);
   hb_ot_metrics_get_position(font->hbFont, HB_OT_METRICS_TAG_CAP_HEIGHT, &font->hbMaxHeight);
}

void FontDeInit(struct Font *font)
{
   hb_font_destroy(font->hbFont);
   hb_face_destroy(font->hbFace);
   hb_gpu_draw_destroy(font->hbDraw);

   free(font->fontPath);
   free(font->glyphCache);
}

void _fontMgrAtlasInit()
{
   struct GlyphAtlas *glyphAtlas = &fm.glyphAtlas;

   glyphAtlas->capacityBytes = ATLAS_PAGE_SIZE;
   glyphAtlas->cursorOffsetBytes = TEXEL_SIZE;
   glyphAtlas->textureUnit = 0;
   glGenBuffers(1, &glyphAtlas->textureBufferObject);
   glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->textureBufferObject);
   glBufferData(GL_TEXTURE_BUFFER, glyphAtlas->capacityBytes, NULL, GL_STATIC_DRAW);

   glActiveTexture(GL_TEXTURE0 + (u32) glyphAtlas->textureUnit);
   glGenTextures(1, &glyphAtlas->texture);
   glBindTexture(GL_TEXTURE_BUFFER, glyphAtlas->texture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, glyphAtlas->textureBufferObject);
}

void _fontMgrAtlasDeInit()
{
   struct GlyphAtlas *glyphAtlas = &fm.glyphAtlas;
   glDeleteBuffers(1, &glyphAtlas->textureBufferObject);
   glDeleteTextures(1, &glyphAtlas->texture);
}
