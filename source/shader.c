#include <stdio.h>

#include "editor.h"
#include "glad/glad.h"

b8 shaderGetCompileStatus(u32 shaderObject)
{
   i32 compileStatus;
   glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
   if (compileStatus)
      return true;

   i32 logLength;
   glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &logLength);

   char infoLog[logLength];
   glGetShaderInfoLog(shaderObject, logLength, NULL, infoLog);
   fprintf(stderr, "failed to compile shader, error message: %s\n", infoLog);
   return false;
}

b8 shaderGetLinkStatus(u32 shaderProgram)
{
   i32 linkStatus;
   glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
   if (linkStatus)
      return true;

   i32 logLength;
   glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

   char infoLog[logLength];
   glGetProgramInfoLog(shaderProgram, logLength, NULL, infoLog);
   fprintf(stderr, "failed to link shader program: %s\n", infoLog);
   return false;
}
