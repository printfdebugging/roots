#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "types.h"

#include "hb.h"
#include "hb-gpu.h"
#include "hb-ot.h"
#include "cglm/struct.h"

/* temporary alias */
typedef u32 rune;

/* todo: document it properly */
/* todo: also pass the text offsets, so it's easy to map clicks to cursor position changes */
struct GlyphVertex
{
   f32 x;
   f32 y;
   f32 tx;
   f32 ty;
   f32 nx;
   f32 ny;
   f32 emPerPos;
   u32 atlasOffset;
   u32 hasCursor;

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
   f64 xMin;
   f64 yMin;
   f64 xMax;
   f64 yMax;
};

struct GlyphAtlas
{
   u32 texture;
   i32 textureUnit;
   u32 textureBufferObject;
   u32 capacityBytes;

   /**!
    * note: This doesn't start at 0, but at `TEXEL_SIZE`. Empty glyphs
    * don't have any glyph data, so their `GlyphInfo.atlasOffset` is set
    * to 0, and if we start at 0 here, that would then use the first uploaded
    * glyph for the spaces..
    */
   u32 cursorOffsetBytes;
};

struct GlyphInfo
{
   f64 advance;
   i32 upem;
   u32 atlasOffset;
   b32 empty;
   b32 cached;
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
   i32 hbAscent;
   i32 hbDescent;
   i32 hbMaxHeight;

   struct GlyphInfo *glyphCache;
};

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

   bool initialized;
};

/* fontmanager.c */
void FontMgrInit(char *editorFontPath);
void FontMgrDeInit();
void FontMgrCacheGlyphInfo(struct Font *font, u32 glyphIndex);
struct GlyphAtlas *FontMgrGetAtlas();
struct Font *FontMgrGetFont(const char *filePath);
struct Font *FontMgrGetDefaultFont();

f32 FontMgrGetDefaultFontScale();
f32 FontMgrGetDefaultFontLineHeight();

struct Font *FontMgrGetFontWithRune(rune codepoint);

void FontInit(struct Font *font, const char *filePath);
void FontDeInit(struct Font *font);

void _fontMgrAtlasInit();
void _fontMgrAtlasDeInit();

// void _linePrintChars();
// void _lineSubstituteNewlines();
// void _lineSubstituteTabs();

#endif
