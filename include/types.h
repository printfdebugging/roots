#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/**********************************
 * user defined convenience types *
 *********************************/

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
   u32 atlasOffset;
   u32 runeIdx;
};

struct Extents
{
   f64 xMin;
   f64 yMin;
   f64 xMax;
   f64 yMax;
};

struct Point
{
   f64 x;
   f64 y;
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

#endif
