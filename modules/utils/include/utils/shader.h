#ifndef UTILS_SHADER_H
#define UTILS_SHADER_H

#include <stdint.h>

bool ShaderGetCompileStatus(uint32_t shaderObject);
bool ShaderGetLinkStatus(uint32_t shaderProgram);

#endif
