#include "engine/utils/shader.h"

#include "glad/glad.h"

#include <stdio.h>

bool shader_get_compile_status(uint32_t object) {
	int32_t status;
	glGetShaderiv(object, GL_COMPILE_STATUS, &status);
	if (status)
		return true;

	int32_t length;
	glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);

	char log[length];
	glGetShaderInfoLog(object, length, NULL, log);
	fprintf(stderr, "failed to compile shader, error message: %s\n", log);
	return false;
}

bool shader_get_link_status(uint32_t program) {
	int32_t status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status)
		return true;

	int32_t length;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

	char log[length];
	glGetProgramInfoLog(program, length, NULL, log);
	fprintf(stderr, "failed to link shader program: %s\n", log);
	return false;
}
