#include "editor.h"

#include "glad/glad.h"

static i32 CURRENT_SHADER_PROGRAM = 0;

static void _rendererUseShaderProgram(i32 shaderProgram)
{
   assert(shaderProgram != 0);
   if (shaderProgram != CURRENT_SHADER_PROGRAM)
   {
      glUseProgram(shaderProgram);
      CURRENT_SHADER_PROGRAM = shaderProgram;
   }
}

void _fontRendererInitGlyphAtlas(struct FontRenderer *renderer)
{
   /************************************************************
    * opengl: create an atlas texture to upload the glyph data *
    ***********************************************************/

   renderer->atlasCapacityBytes     = ATLAS_PAGE_SIZE;
   renderer->atlasCursorOffsetBytes = 0;
   glGenBuffers(1, &renderer->atlasTextureBufferObject);
   glBindBuffer(GL_TEXTURE_BUFFER, renderer->atlasTextureBufferObject);
   glBufferData(GL_TEXTURE_BUFFER, renderer->atlasCapacityBytes, NULL, GL_STATIC_DRAW);

   glActiveTexture(renderer->atlasTextureUnit);
   glGenTextures(1, &renderer->atlasTexture);
   glBindTexture(GL_TEXTURE_BUFFER, renderer->atlasTexture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, renderer->atlasTextureBufferObject);
}

void fontRendererInit(struct FontRenderer *renderer)
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
   renderer->viewport          = GLMS_IVEC4_ZERO;
   renderer->scale             = 0;
   renderer->position          = GLMS_VEC2_ZERO;
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

   _fontRendererInitGlyphAtlas(renderer);
}

void fontRendererDeInit(struct FontRenderer *renderer)
{
   glDeleteBuffers(1, &renderer->glyphQuadVerticesVBO);
   glDeleteVertexArrays(1, &renderer->glyphQuadVerticesVAO);
   glDeleteProgram(renderer->hbShaderProgram);
}

void fontRendererCacheUniformLoc(struct FontRenderer *renderer)
{
   /******************************************
    * opengl: cache shader uniform locations *
    *****************************************/
   _rendererUseShaderProgram(renderer->hbShaderProgram);
   renderer->matViewProjectionLoc = glGetUniformLocation(renderer->hbShaderProgram, "u_matViewProjection");
   renderer->viewportLoc          = glGetUniformLocation(renderer->hbShaderProgram, "u_viewport");
   renderer->scaleLoc             = glGetUniformLocation(renderer->hbShaderProgram, "u_scale");
   renderer->positionLoc          = glGetUniformLocation(renderer->hbShaderProgram, "u_position");
   renderer->gammaLoc             = glGetUniformLocation(renderer->hbShaderProgram, "u_gamma");
   renderer->foregroundLoc        = glGetUniformLocation(renderer->hbShaderProgram, "u_foreground");
   renderer->debugLoc             = glGetUniformLocation(renderer->hbShaderProgram, "u_debug");
   renderer->stemDarkeningLoc     = glGetUniformLocation(renderer->hbShaderProgram, "u_stem_darkening");
   renderer->hbGpuAtlasLoc        = glGetUniformLocation(renderer->hbShaderProgram, "hb_gpu_atlas");
   renderer->runeIdxLoc           = glGetUniformLocation(renderer->hbShaderProgram, "u_runeIdx");
}

void fontRendererUploadUniforms(struct FontRenderer *renderer)
{
   _rendererUseShaderProgram(renderer->hbShaderProgram);
   glUniformMatrix4fv(renderer->matViewProjectionLoc, 1, GL_FALSE, renderer->matViewProjection.col[0].raw);
   glUniform4fv(renderer->foregroundLoc, 1, renderer->foreground.raw);
   glUniform2fv(renderer->positionLoc, 1, renderer->position.raw);
   glUniform2f(renderer->viewportLoc, (f32) renderer->viewport.raw[2], (f32) renderer->viewport.raw[3]);
   glUniform1f(renderer->scaleLoc, (f32) renderer->scale);
   glUniform1f(renderer->stemDarkeningLoc, renderer->stemDarkening);
   glUniform1f(renderer->runeIdxLoc, renderer->runeIdx);
   glUniform1f(renderer->debugLoc, renderer->debug);
   glUniform1f(renderer->gammaLoc, renderer->gamma);
   glUniform1i(renderer->hbGpuAtlasLoc, (i32) renderer->hbGpuAtlas);
}
