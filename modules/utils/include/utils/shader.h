#ifndef UTILS_SHADER_H
#define UTILS_SHADER_H

#include <stdint.h>

bool shader_get_compile_status(uint32_t object);
bool shader_get_link_status(uint32_t program);

#endif
