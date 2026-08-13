#include "editor.h"

#include "glad/glad.h"

void rendererInit(struct Renderer *renderer)
{
   renderer->hbShaderProgram                                   = 0;
   renderer->hbShaderProgram_UniformLocation_matViewProjection = -1;
   renderer->hbShaderProgram_UniformLocation_viewport          = -1;
   renderer->hbShaderProgram_UniformLocation_scale             = -1;
   renderer->hbShaderProgram_UniformLocation_position          = -1;
   renderer->hbShaderProgram_UniformLocation_hb_gpu_atlas      = -1;
   renderer->hbShaderProgram_UniformLocation_gamma             = -1;
   renderer->hbShaderProgram_UniformLocation_foreground        = -1;
   renderer->hbShaderProgram_UniformLocation_debug             = -1;
   renderer->hbShaderProgram_UniformLocation_stem_darkening    = -1;
   renderer->hbShaderProgram_UniformLocation_runeIdx           = -1;

   /**********************************
    * renderer - draw uniform states *
    *********************************/
   renderer->hbUniform_matViewProjection = (mat4s) { GLM_MAT4_IDENTITY_INIT };
   renderer->hbUniform_viewport          = (ivec4s) { 0 };
   renderer->hbUniform_scale             = 0;
   renderer->hbUniform_position          = (vec2s) { 0 };
   renderer->hbUniform_hb_gpu_atlas      = 0;
   renderer->hbUniform_gamma             = 0;
   renderer->hbUniform_foreground        = (vec4s) { ColorRGBAHex(0XD8DEE9FF) };
   renderer->hbUniform_debug             = false;
   renderer->hbUniform_stem_darkening    = false;
   renderer->hbUniform_runeIdx           = 0;

   /*********************************
    * renderer - object store/cache *
    ********************************/
   renderer->atlasTexture             = 0;
   renderer->atlasTextureUnit         = GL_TEXTURE0;
   renderer->atlasTextureBufferObject = 0;
   renderer->atlasCapacityBytes       = 0;
   renderer->atlasCursorOffsetBytes   = 0;

   /**************************
    * renderer - layout data *
    *************************/
   renderer->glyphQuadVerticesVAO = 0;
   renderer->glyphQuadVerticesVBO = 0;
   renderer->glyphQuadsUploaded   = false;
}

void rendererDeInit(struct Renderer *renderer)
{
   glDeleteBuffers(1, &renderer->glyphQuadVerticesVBO);
   glDeleteVertexArrays(1, &renderer->glyphQuadVerticesVAO);
   glDeleteProgram(renderer->hbShaderProgram);
}
