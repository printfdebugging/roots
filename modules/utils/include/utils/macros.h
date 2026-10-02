#ifndef UTILS_MACROS_H
#define UTILS_MACROS_H

#define color_rgb_hex(color)             \
   (((color >> 16) & 0xFF) / 255.0f),    \
       (((color >> 8) & 0xFF) / 255.0f), \
       (((color) & 0xFF) / 255.0f)

#define color_rgba_hex(color)         \
   (((color >> 24) & 0xFF) / 255.0f), \
       color_rgb_hex(color)

#define array_size(t) (sizeof(t) / sizeof(*t))

#endif
