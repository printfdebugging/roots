#ifndef MACROS_H
#define MACROS_H

/***************
 *    macros   *
 ***************/

#define ColorRGBHex(color)               \
   (((color >> 16) & 0xFF) / 255.0f),    \
       (((color >> 8) & 0xFF) / 255.0f), \
       (((color) & 0xFF) / 255.0f)

#define ColorRGBAHex(color)           \
   (((color >> 24) & 0xFF) / 255.0f), \
       ColorRGBHex(color)

#define U64_MAX 18446744073709551615UL
#define U32_MAX 4294967295U
#define U16_MAX 65535U
#define U8_MAX  255U
#define U64_MIN 0UL
#define U32_MIN 0U
#define U16_MIN 0U
#define U8_MIN  0U

#define I8_MAX  127
#define I16_MAX 32767
#define I32_MAX 2147483647
#define I64_MAX 9223372036854775807L
#define I8_MIN  (-I8_MAX - 1)
#define I16_MIN (-I16_MAX - 1)
#define I32_MIN (-I32_MAX - 1)
#define I64_MIN (-I64_MAX - 1)

#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)

#define MM   (1.0f)
#define CM   (10.0f MM)
#define INCH (25.4f * MM)

#define TEXEL_SIZE        8
#define ATLAS_PAGE_SIZE   (TEXEL_SIZE * MB)
#define MAX_TEXTURE_COUNT 16

#define ArraySize(t) (sizeof(t) / sizeof(*t))

#endif
