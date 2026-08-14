#include <string.h>

#include "hb-ot.h"
#include "glad/glad.h"

#include "editor.h"

/**!
 * Font manager is a subsystem we request for the font objects.
 * This way, we don't have to manage the lifetime of these objects. And
 * since these objects are shared, so is the glyphCache.
 */

struct FontManager
{
   struct Font *font;
   u32 fontCount;

   /**!
    * Path the default editor font.
    */
   const char *editorFontPath;

   /**!
    * The default font of the editor. Every rune is first shaped
    * with this font and if it doesn't have a glyph, we check other
    * cached fonts then the system fonts using fontconfig.
    */
   struct Font *editorFont;

   /**!
    * OpenGL textures with the glyph data. `GlyphInfo.atlasOffset` is an
    * offset into this texture. We only cache the glyphs being used, so
    * even if we are using a few fonts, it should be fine for the most part.
    */
   struct GlyphAtlas glyphAtlas;

   b8 initialized;
};

static struct FontManager fontManager = { 0 };

void _fontManagerGlyphAtlasInit();
void _fontManagerGlyphAtlasDeInit();

void fontManagerInit(char *editorFontPath)
{
   /**!
    * `fontManagerGetFont` checks the `initialized` flag and
    * returns early if not, so we should mark it early for the
    * below call load/return the font
    */
   fontManager.initialized = true;

   _fontManagerGlyphAtlasInit();
   fontManager.editorFontPath = stringDuplicate(editorFontPath);
   fontManager.editorFont     = fontManagerGetFont(editorFontPath);
}

void fontManagerDeInit()
{
   if (!fontManager.initialized)
      return;

   for (u32 fontIdx = 0; fontIdx < fontManager.fontCount; ++fontIdx)
      fontDeInit(&fontManager.font[fontIdx]);

   free(fontManager.font);
   free((void *) fontManager.editorFontPath);

   _fontManagerGlyphAtlasDeInit();
   fontManager.initialized = false;
}

void fontManagerMakeLineGlyphInfoSpec(struct LineGlyphInfo *lineGlyphInfo, char *lineUTF8, u32 lineByteLen)
{
   (void) lineByteLen;
   if (!fontManager.initialized)
      return;

   struct Font *font = fontManagerGetDefaultFont();

   hb_buffer_t *buffer = hb_buffer_create();
   hb_buffer_add_utf8(buffer, lineUTF8, -1, 0, -1);
   hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
   hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
   hb_shape(font->hbFont, buffer, NULL, 0);

   u32 glyphCount              = 0;
   hb_glyph_info_t *glyphInfos = hb_buffer_get_glyph_infos(buffer, &glyphCount);

   lineGlyphInfo->glyphInfo  = realloc(lineGlyphInfo->glyphInfo, glyphCount * sizeof(struct GlyphInfo));
   lineGlyphInfo->glyphCount = glyphCount;
   memset(lineGlyphInfo->glyphInfo, 0, glyphCount * sizeof(struct GlyphInfo));

   for (u32 glyphIdx = 0; glyphIdx < glyphCount; ++glyphIdx)
   {
      hb_codepoint_t glyphIndex = glyphInfos[glyphIdx].codepoint;
      if (!font->glyphCache[glyphIndex].cached)
      {
         i32 xScale, yScale;
         hb_font_get_scale(font->hbFont, &xScale, &yScale);
         hb_gpu_draw_clear(font->hbDraw);
         hb_gpu_draw_glyph(font->hbDraw, font->hbFont, glyphIndex);

         hb_glyph_extents_t hbGlyphExtents = {};
         hb_blob_t *hbBlob                 = NULL;

         hbBlob           = hb_gpu_draw_encode(font->hbDraw, &hbGlyphExtents);
         u32 hbBlobLength = hbBlob ? hb_blob_get_length(hbBlob) : 0;

         /******************************************************************
          * todo: check if we got an empty glyph and if we did then        *
          * find a font either in the `fontManager.fonts` or on the system *
          * (using `fontconfig`) and cache that font.                      *
          *****************************************************************/

         /*****************************
          * cache the glyph quad info *
          ****************************/

         font->glyphCache[glyphIndex] = (struct GlyphInfo) {
            .extents.xMin = 0,
            .extents.xMax = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
            .extents.yMin = font->hbDescent,
            .extents.yMax = font->hbAscent,
            .advance      = hb_font_get_glyph_h_advance(font->hbFont, glyphIndex),
            .upem         = yScale,
            .empty        = (hbBlobLength == 0),
            .cached       = true,
         };

         /*********************************************************
          * upload glyph primitives to the gpu & store the offset *
          ********************************************************/

         struct GlyphAtlas *glyphAtlas = fontManagerGetGlyphAtlas();
         if (!font->glyphCache[glyphIndex].empty)
         {
            const char *hbGlyphData = hb_blob_get_data(hbBlob, NULL);
            glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->textureBufferObject);
            glBufferSubData(GL_TEXTURE_BUFFER, glyphAtlas->cursorOffsetBytes, hbBlobLength, hbGlyphData);
            font->glyphCache[glyphIndex].atlasOffset = glyphAtlas->cursorOffsetBytes;
            glyphAtlas->cursorOffsetBytes += hbBlobLength;

            hb_gpu_draw_recycle_blob(font->hbDraw, hbBlob);
         }
      }

      lineGlyphInfo->glyphInfo[glyphIdx] = font->glyphCache[glyphIndex];
   }

   hb_buffer_destroy(buffer);
}

struct GlyphAtlas *fontManagerGetGlyphAtlas()
{
   if (!fontManager.initialized)
      return NULL;
   return &fontManager.glyphAtlas;
}

struct Font *fontManagerGetFont(const char *filePath)
{
   if (!fontManager.initialized)
      return NULL;

   for (u32 fontIdx = 0; fontIdx < fontManager.fontCount; ++fontIdx)
      if (strcmp(fontManager.font[fontIdx].fontPath, filePath) == 0)
         return &fontManager.font[fontIdx];

   fontManager.font  = realloc(fontManager.font, sizeof(struct Font) * (fontManager.fontCount + 1));
   struct Font *font = &fontManager.font[fontManager.fontCount++];
   fontInit(font, filePath);

   return font;
}

struct Font *fontManagerGetDefaultFont()
{
   if (!fontManager.initialized)
      return NULL;
   return fontManager.editorFont;
}

struct Font *fontManagerGetFontWithRune(rune codepoint)
{
   (void) codepoint;
   perror("todo");
   return NULL;
}

void fontInit(struct Font *font, const char *filePath)
{
   font->fontPath   = stringDuplicate(filePath);
   font->glyphCache = calloc(U16_MAX, sizeof(struct GlyphInfo));

   /*********************************************************
    * harfbuzz: font loading & shape encoder initialization *
    ********************************************************/

   hb_blob_t *hbBlob = NULL;
   if (!(hbBlob = hb_blob_create_from_file(font->fontPath)) ||
       !(font->hbFace = hb_face_create(hbBlob, 0)) ||
       !(font->hbFont = hb_font_create(font->hbFace)) ||
       !(font->hbDraw = hb_gpu_draw_create_or_fail()))
   {
      perror("failed to initialize harfbuzz");
   }

   hb_blob_destroy(hbBlob);

   const hb_ot_metrics_tag_t ASCENT_HHEA  = HB_TAG('H', 'a', 's', 'c');
   const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

   hb_ot_metrics_get_position(font->hbFont, ASCENT_HHEA, &font->hbAscent);
   hb_ot_metrics_get_position(font->hbFont, DESCENT_HHEA, &font->hbDescent);
   hb_ot_metrics_get_position(font->hbFont, HB_OT_METRICS_TAG_CAP_HEIGHT, &font->hbMaxHeight);
}

void fontDeInit(struct Font *font)
{
   hb_font_destroy(font->hbFont);
   hb_face_destroy(font->hbFace);
   hb_gpu_draw_destroy(font->hbDraw);

   free(font->fontPath);
   free(font->glyphCache);
}

void _fontManagerGlyphAtlasInit()
{
   /************************************************************
    * opengl: create an atlas texture to upload the glyph data *
    ***********************************************************/
   struct GlyphAtlas *glyphAtlas = &fontManager.glyphAtlas;

   glyphAtlas->capacityBytes     = ATLAS_PAGE_SIZE;
   glyphAtlas->cursorOffsetBytes = 0;
   glGenBuffers(1, &glyphAtlas->textureBufferObject);
   glBindBuffer(GL_TEXTURE_BUFFER, glyphAtlas->textureBufferObject);
   glBufferData(GL_TEXTURE_BUFFER, glyphAtlas->capacityBytes, NULL, GL_STATIC_DRAW);

   glActiveTexture(glyphAtlas->textureUnit);
   glGenTextures(1, &glyphAtlas->texture);
   glBindTexture(GL_TEXTURE_BUFFER, glyphAtlas->texture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, glyphAtlas->textureBufferObject);
}

void _fontManagerGlyphAtlasDeInit()
{
   struct GlyphAtlas *glyphAtlas = &fontManager.glyphAtlas;
   glDeleteBuffers(1, &glyphAtlas->textureBufferObject);
   glDeleteTextures(1, &glyphAtlas->texture);
}
