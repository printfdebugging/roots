#include "editor.h"

#include "glad/glad.h"

static u32 CURRENT_SHADER_PROGRAM = 0;

static void _rendererUseShaderProgram(u32 shaderProgram)
{
   assert(shaderProgram != 0);
   if (shaderProgram != CURRENT_SHADER_PROGRAM)
   {
      glUseProgram(shaderProgram);
      CURRENT_SHADER_PROGRAM = shaderProgram;
   }
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
   renderer->foreground        = (vec4s) { { ColorRGBAHex(0XD8DEE9FF) } };
   renderer->debug             = false;
   renderer->stemDarkening     = false;
   renderer->runeIdx           = 0;

   /**************************
    * renderer - layout data *
    *************************/
   glGenVertexArrays(1, &renderer->glyphQuadVerticesVAO);
   glGenBuffers(1, &renderer->glyphQuadVerticesVBO);
   renderer->glyphQuadsUploaded = false;
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
   glUniform1i(renderer->runeIdxLoc, renderer->runeIdx);
   glUniform1f(renderer->debugLoc, renderer->debug);
   glUniform1f(renderer->gammaLoc, renderer->gamma);
   glUniform1i(renderer->hbGpuAtlasLoc, (i32) renderer->hbGpuAtlas);
}

void fontRendererCreateShader(struct FontRenderer *renderer)
{
   /******************************************************************
    * opengl: create a shader `hbShaderProgram` for rendering glyphs *
    *****************************************************************/

   const char *hbShaderVersion  = "#version 330 core\n";
   const char *hbShaderPreamble = "#define HB_GPU_DEMO_DRAW\n";
   const char *hbVertexMain     = readFileContents(ASSETS_DIR "harfbuzz.vert");
   const char *hbFragmentMain   = readFileContents(ASSETS_DIR "harfbuzz.frag");

   u32 hbVertexShader;
   u32 hbFragmentShader;

   const char *hbVertexShaderSources[] = {
      hbShaderVersion,
      hbShaderPreamble,
      hb_gpu_shader_source(HB_GPU_SHADER_STAGE_VERTEX, HB_GPU_SHADER_LANG_GLSL),
      hb_gpu_draw_shader_source(HB_GPU_SHADER_STAGE_VERTEX, HB_GPU_SHADER_LANG_GLSL),
      hbVertexMain,
   };

   const char *hbFragmentShaderSources[] = {
      hbShaderVersion,
      hbShaderPreamble,
      hb_gpu_shader_source(HB_GPU_SHADER_STAGE_FRAGMENT, HB_GPU_SHADER_LANG_GLSL),
      hb_gpu_draw_shader_source(HB_GPU_SHADER_STAGE_FRAGMENT, HB_GPU_SHADER_LANG_GLSL),
      hbFragmentMain,
   };

   hbVertexShader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(hbVertexShader, ArraySize(hbVertexShaderSources), hbVertexShaderSources, NULL);
   glCompileShader(hbVertexShader);
   if (!shaderGetCompileStatus(hbVertexShader))
      perror("vertex shader compilation failed");

   hbFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(hbFragmentShader, ArraySize(hbFragmentShaderSources), hbFragmentShaderSources, NULL);
   glCompileShader(hbFragmentShader);
   if (!shaderGetCompileStatus(hbFragmentShader))
      perror("fragment shader compilation failed");

   renderer->hbShaderProgram = glCreateProgram();
   glAttachShader(renderer->hbShaderProgram, hbVertexShader);
   glAttachShader(renderer->hbShaderProgram, hbFragmentShader);
   glLinkProgram(renderer->hbShaderProgram);
   if (!shaderGetLinkStatus(renderer->hbShaderProgram))
      perror("failed to link shader program");

   glDeleteShader(hbVertexShader);
   glDeleteShader(hbFragmentShader);
   free((void *) hbVertexMain);
   free((void *) hbFragmentMain);
}

void fontRendererSetupAttribLocations(struct FontRenderer *renderer)
{
   /**********************************************************
    * opengl: setup attribute locations in `hbShaderProgram` *
    *********************************************************/
   i32 glyphQuadObjectStride = sizeof(struct GlyphVertex);
   i32 attribLocation        = -1;

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_position");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, x));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_texcoord");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, tx));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_normal");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, nx));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_emPerPos");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 1, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, emPerPos));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_glyphLoc");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, atlasOffset));

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_runeIdx");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, runeIdx));
}

void fontRendererUploadLayoutQuadsToGPU(struct FontRenderer *renderer, struct FontLayout *layout)
{
   glBindVertexArray(renderer->glyphQuadVerticesVAO);
   glBindBuffer(GL_ARRAY_BUFFER, renderer->glyphQuadVerticesVBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * layout->glyphQuadVerticesCount, layout->glyphQuadVertices, GL_STATIC_DRAW);
   renderer->glyphQuadsUploaded = true;
}
