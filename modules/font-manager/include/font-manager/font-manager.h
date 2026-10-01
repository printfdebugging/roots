#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "hb.h"
#include "hb-gpu.h"
#include "hb-ot.h"
#include "cglm/struct.h"

#include <stdint.h>

/* todo: document it properly */
/* todo: also pass the text offsets, so it's easy to map clicks to cursor position changes */
struct GlyphVertex
{
   float x;
   float y;
   float tx;
   float ty;
   float nx;
   float ny;
   float emPerPos;
   uint32_t atlasOffset;
   uint32_t hasCursor;

   /*
   todo:
      vec4 textPos;
         [0] = textId
         [1] = line
         [2] = byteOffset
   */

   vec4s fgColor;
   vec4s bgColor;
};

struct Extents
{
   double xMin;
   double yMin;
   double xMax;
   double yMax;
};

struct GlyphAtlas
{
   uint32_t texture;
   int32_t textureUnit;
   uint32_t textureBufferObject;
   uint32_t capacityBytes;

   /**!
    * note: This doesn't start at 0, but at `TEXEL_SIZE`. Empty glyphs
    * don't have any glyph data, so their `GlyphInfo.atlasOffset` is set
    * to 0, and if we start at 0 here, that would then use the first uploaded
    * glyph for the spaces..
    */
   uint32_t cursorOffsetBytes;
};

struct GlyphInfo
{
   double advance;
   int32_t upem;
   uint32_t atlasOffset;
   bool empty;
   bool cached;
   struct Extents extents;
};

struct Font
{
   char *fontPath;

   /* font objects & the encoder */
   hb_face_t *hbFace;
   hb_font_t *hbFont;
   hb_gpu_draw_t *hbDraw;

   /* font metrics */
   int32_t hbAscent;
   int32_t hbDescent;
   int32_t hbMaxHeight;

   struct GlyphInfo *glyphCache;
};

struct FontManager
{
   struct Font *font;
   uint32_t fontCount;

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

   bool initialized;
};

/* fontmanager.c */
void FontMgrInit(char *editorFontPath);
void FontMgrDeInit();
void FontMgrCacheGlyphInfo(struct Font *font, uint32_t glyphIndex);
struct GlyphAtlas *FontMgrGetAtlas();
struct Font *FontMgrGetFont(const char *filePath);
struct Font *FontMgrGetDefaultFont();

float FontMgrGetDefaultFontScale();
float FontMgrGetDefaultFontLineHeight();

struct Font *FontMgrGetFontWithRune(uint32_t codepoint);

void FontInit(struct Font *font, const char *filePath);
void FontDeInit(struct Font *font);

void _fontMgrAtlasInit();
void _fontMgrAtlasDeInit();

// void _linePrintChars();
// void _lineSubstituteNewlines();
// void _lineSubstituteTabs();

#endif
