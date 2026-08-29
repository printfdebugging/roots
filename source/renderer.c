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

void lineRendererInit(struct LineRenderer *renderer)
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

   renderer->rendererOpts = (struct LineRendererOptions) {
      .matViewProjection = (mat4s) { GLM_MAT4_IDENTITY_INIT },
      .viewport          = GLMS_IVEC4_ZERO,
      .scale             = 0,
      .position          = GLMS_VEC2_ZERO,
      .hbGpuAtlas        = 0,
      .gamma             = 0,
      .foreground        = (vec4s) { { ColorRGBAHex(0XD8DEE9FF) } },
      .debug             = false,
      .stemDarkening     = false,
   };

   /**************************
    * renderer - layout data *
    *************************/
   glGenVertexArrays(1, &renderer->glyphQuadVerticesVAO);
   glGenBuffers(1, &renderer->glyphQuadVerticesVBO);
   renderer->glyphQuadVerticesCount = 0;
   renderer->glyphQuadsUploaded     = false;
}

void lineRendererDeInit(struct LineRenderer *renderer)
{
   glDeleteBuffers(1, &renderer->glyphQuadVerticesVBO);
   glDeleteVertexArrays(1, &renderer->glyphQuadVerticesVAO);
   glDeleteProgram(renderer->hbShaderProgram);
}

void lineRendererCacheUniformLoc(struct LineRenderer *renderer)
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
}

void lineRendererUploadUniforms(struct LineRenderer *renderer)
{
   _rendererUseShaderProgram(renderer->hbShaderProgram);
   struct LineRendererOptions opts = renderer->rendererOpts;

   glUniformMatrix4fv(renderer->matViewProjectionLoc, 1, GL_FALSE, opts.matViewProjection.col[0].raw);
   glUniform4fv(renderer->foregroundLoc, 1, opts.foreground.raw);
   glUniform2fv(renderer->positionLoc, 1, opts.position.raw);
   glUniform2f(renderer->viewportLoc, (f32) opts.viewport.raw[2], (f32) opts.viewport.raw[3]);
   glUniform1f(renderer->scaleLoc, (f32) opts.scale);
   glUniform1f(renderer->stemDarkeningLoc, opts.stemDarkening);
   glUniform1f(renderer->debugLoc, opts.debug);
   glUniform1f(renderer->gammaLoc, opts.gamma);
   glUniform1i(renderer->hbGpuAtlasLoc, (i32) opts.hbGpuAtlas);
}

void lineRendererRenderLine(struct LineRenderer *renderer)
{
   glDrawArrays(GL_TRIANGLES, 0, (i32) renderer->glyphQuadVerticesCount);
}

void lineRendererCreateShader(struct LineRenderer *renderer)
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

void lineRendererSetupAttribLocations(struct LineRenderer *renderer)
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

   attribLocation = glGetAttribLocation(renderer->hbShaderProgram, "a_hasCursor");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, hasCursor));
}

void lineRendererUploadLayoutQuadsToGPU(struct LineRenderer *renderer, struct LineLayout *layout)
{
   glBindVertexArray(renderer->glyphQuadVerticesVAO);
   glBindBuffer(GL_ARRAY_BUFFER, renderer->glyphQuadVerticesVBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * layout->glyphQuadVerticesCount, layout->glyphQuadVertices, GL_STATIC_DRAW);
   renderer->glyphQuadVerticesCount = layout->glyphQuadVerticesCount;
   renderer->glyphQuadsUploaded     = true;
}
