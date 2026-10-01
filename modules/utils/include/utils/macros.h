#ifndef UTILS_MACROS_H
#define UTILS_MACROS_H

#define ColorRGBHex(color)               \
   (((color >> 16) & 0xFF) / 255.0f),    \
       (((color >> 8) & 0xFF) / 255.0f), \
       (((color) & 0xFF) / 255.0f)

#define ColorRGBAHex(color)           \
   (((color >> 24) & 0xFF) / 255.0f), \
       ColorRGBHex(color)

#define ArraySize(t) (sizeof(t) / sizeof(*t))

#endif
