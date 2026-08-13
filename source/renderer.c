#include "editor.h"

#include "glad/glad.h"

void rendererInit(struct Renderer *renderer)
{
   renderer->hbShaderProgram      = 0;
   renderer->matViewProjectionLoc = -1;
   renderer->viewportLoc          = -1;
   renderer->scaleLoc             = -1;
   renderer->positionLoc          = -1;
   renderer->hbGpuAtlasLoc        = -1;
   renderer->gammaLoc             = -1;
   renderer->foregroundLoc        = -1;
   renderer->debugLoc             = -1;
   renderer->stemDarkeningLoc     = -1;
   renderer->runeIdxLoc           = -1;

   /**********************************
    * renderer - draw uniform states *
    *********************************/
   renderer->matViewProjection = (mat4s) { GLM_MAT4_IDENTITY_INIT };
   renderer->viewport          = (ivec4s) { 0 };
   renderer->scale             = 0;
   renderer->position          = (vec2s) { 0 };
   renderer->hbGpuAtlas        = 0;
   renderer->gamma             = 0;
   renderer->foreground        = (vec4s) { ColorRGBAHex(0XD8DEE9FF) };
   renderer->debug             = false;
   renderer->stemDarkening     = false;
   renderer->runeIdx           = 0;

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
