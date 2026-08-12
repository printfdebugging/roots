#ifndef SHADER_H
#define SHADER_H

#include <stdio.h>

#include "types.h"

#include "glad/glad.h"

b8 shaderGetCompileStatus(u32 shaderObject);
b8 shaderGetLinkStatus(u32 shaderProgram);

#endif
