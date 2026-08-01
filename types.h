#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/**********************************
 * user defined convenience types *
 **********************************/

typedef uint_least8_t u8;
typedef uint_least16_t u16;
typedef uint_least32_t u32;
typedef uint_least64_t u64;

typedef int_least8_t i8;
typedef int_least16_t i16;
typedef int_least32_t i32;
typedef int_least64_t i64;

typedef float f32;
typedef double f64;
typedef bool b8;
typedef int b32;

struct GlyphVertex
{
   f32 x;
   f32 y;
   f32 tx;
   f32 ty;
   f32 nx;
   f32 ny;
   f32 emPerPos;
   u32 atlas_offset;
};

struct Extents
{
   f64 min_x;
   f64 min_y;
   f64 max_x;
   f64 max_y;
};

struct Point
{
   f64 x;
   f64 y;
};

/** This is unmodified glyph info as we got from the font. We transform this and
 * create a `glyph_vertex` with scaling / coordinate conversion. todo: lookup and
 * write here what exactly we need to do. */
struct GlyphInfo
{
   struct Extents extents;
   f64 advance;

   /** `upem` is the dpi of the display, or more specifically how points
    * map to pixels. This is `unused` since we directly use the dpi to
    * render the text. We don't scale the glyph quads on cpu, that's done
    * in the shader using a `u_scale` uniform. that way we can easily change
    * the font size without invalidating the glyph quads.
    */
   i32 upem;

   /** These are the glyph's primitive locations in the texture on the gpu.
    * `atlas_upload_glyph` sets these after uploading the glyph. `atlas_page`
    * is for the `texture_unit` which has the glyph primitives and `atlas_offset`
    * is the offset in `bytes` in that texture. */
   u32 atlas_offset;

   b32 empty;

   /** This is allocated in bulk and ideally with `calloc`. That way `cached`
    * will be set to `false` or `0` by default and then we can check for that
    * to see if the glyph is there or not.
    */
   b32 cached;
};

#endif
