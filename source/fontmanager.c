#include <string.h>

#include "hb-ot.h"

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
   b8 initialized;
};

static struct FontManager fontManager = { 0 };

void fontManagerInit()
{
   fontManager.initialized = true;
}

void fontManagerDeInit()
{
   if (!fontManager.initialized)
      return;

   for (u32 fontIdx = 0; fontIdx < fontManager.fontCount; ++fontIdx)
      fontDeInit(&fontManager.font[fontIdx]);
   free(fontManager.font);

   fontManager.initialized = false;
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

struct Font *fontManagerGetFontWithRune(rune codepoint)
{
   perror("todo");
   return NULL;
}

void fontInit(struct Font *font, const char *filePath)
{
   i32 fontPathLen = strlen(filePath);
   assert(fontPathLen != 0);
   font->fontPath = calloc(fontPathLen + 1, sizeof(char));
   strcpy(font->fontPath, filePath);

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
