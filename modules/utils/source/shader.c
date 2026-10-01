#include "utils/shader.h"

#include "glad/glad.h"

#include <stdio.h>

bool ShaderGetCompileStatus(uint32_t shaderObject)
{
   int32_t compileStatus;
   glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
   if (compileStatus)
      return true;

   int32_t logLength;
   glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &logLength);

   char infoLog[logLength];
   glGetShaderInfoLog(shaderObject, logLength, NULL, infoLog);
   fprintf(stderr, "failed to compile shader, error message: %s\n", infoLog);
   return false;
}

bool ShaderGetLinkStatus(uint32_t shaderProgram)
{
   int32_t linkStatus;
   glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
   if (linkStatus)
      return true;

   int32_t logLength;
   glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

   char infoLog[logLength];
   glGetProgramInfoLog(shaderProgram, logLength, NULL, infoLog);
   fprintf(stderr, "failed to link shader program: %s\n", infoLog);
   return false;
}
