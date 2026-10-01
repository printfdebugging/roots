#include "editor.h"

#include "glad/glad.h"
#include "utils/macros.h"
#include "utils/string.h"
#include "utils/shader.h"

static uint32_t CURRENT_SHADER_PROGRAM = 0;

static uint32_t _createTextShader();
static void _rendererUseShaderProgram(uint32_t shaderProgram);

/* this has to now happen once for each BufferRenderer (doens't exist yet.) */
/* todo: warning: refactor it asap */
void InitBufferRenderer(struct BufferRenderer *renderer, struct TextShader *shader)
{
   /* uniforms */
   renderer->uniforms = (struct TextShaderUniforms) {
      .matViewProjection = (mat4s) { GLM_MAT4_IDENTITY_INIT },
      .viewport = GLMS_IVEC4_ZERO,
      .scale = 0,
      .hbGpuAtlas = 0,
      .gamma = 0,
      .debug = false,
      .stemDarkening = false,
   };

   /* primitives */
   glGenVertexArrays(1, &renderer->vao);
   glGenBuffers(1, &renderer->vbo);
   renderer->count = 0;
   renderer->uploaded = false;

   /**!
    * note: We do this here because it is only done once per
    * VAO and what best place could be to do it than the Init
    * function?
    *
    * warn: Though this might change later if we decide to
    * have struct of arrays rather than array of structs..
    */
   glBindVertexArray(renderer->vao);
   glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);

   /* set attribute locations */

   uint32_t program = shader->hbShaderProgram;
   int32_t attribLocation = -1;
   int32_t glyphQuadObjectStride = sizeof(struct GlyphVertex);

   /**!
    * warning: These should be called only after the buffer object
    * is bound to the array buffer, otherwise it can lead to bugs like all
    * the vertex array buffers using the same vertex buffer object which
    * was set when this function was called.
    */

   attribLocation = glGetAttribLocation(program, "a_position");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribPointer((uint32_t) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, x));

   attribLocation = glGetAttribLocation(program, "a_texcoord");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribPointer((uint32_t) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, tx));

   attribLocation = glGetAttribLocation(program, "a_normal");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribPointer((uint32_t) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, nx));

   attribLocation = glGetAttribLocation(program, "a_emPerPos");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribPointer((uint32_t) attribLocation, 1, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, emPerPos));

   attribLocation = glGetAttribLocation(program, "a_glyphLoc");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribIPointer((uint32_t) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, atlasOffset));

   attribLocation = glGetAttribLocation(program, "a_hasCursor");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribIPointer((uint32_t) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, hasCursor));

   attribLocation = glGetAttribLocation(program, "a_fgColor");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribIPointer((uint32_t) attribLocation, 4, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, fgColor));

   attribLocation = glGetAttribLocation(program, "a_bgColor");
   glEnableVertexAttribArray((uint32_t) attribLocation);
   glVertexAttribIPointer((uint32_t) attribLocation, 4, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, bgColor));
}

/* todo: this also needs fixing asap */
void DeInitBufferRenderer(struct BufferRenderer *renderer)
{
   /* primitives */
   renderer->uploaded = false;
   renderer->count = 0;
   glDeleteBuffers(1, &renderer->vbo);
   glDeleteVertexArrays(1, &renderer->vao);
}

void CreateTextShader(struct TextShader *shader)
{
   shader->hbShaderProgram = _createTextShader();
   shader->uniformLocations = (struct TextShaderUniformLocations) {
      .matViewProjectionLoc = -1,
      .viewportLoc = -1,
      .scaleLoc = -1,
      .positionLoc = -1,
      .hbGpuAtlasLoc = -1,
      .gammaLoc = -1,
      .debugLoc = -1,
      .stemDarkeningLoc = -1,
   };

   uint32_t program = shader->hbShaderProgram;
   _rendererUseShaderProgram(program);

   shader->uniformLocations = (struct TextShaderUniformLocations) {
      .matViewProjectionLoc = glGetUniformLocation(program, "u_matViewProjection"),
      .viewportLoc = glGetUniformLocation(program, "u_viewport"),
      .scaleLoc = glGetUniformLocation(program, "u_scale"),
      .positionLoc = glGetUniformLocation(program, "u_position"),
      .gammaLoc = glGetUniformLocation(program, "u_gamma"),
      .debugLoc = glGetUniformLocation(program, "u_debug"),
      .stemDarkeningLoc = glGetUniformLocation(program, "u_stem_darkening"),
      .hbGpuAtlasLoc = glGetUniformLocation(program, "hb_gpu_atlas"),
   };
}

void DestroyTextShader(struct TextShader *shader)
{
   glDeleteProgram(shader->hbShaderProgram);
}

void lineShaderDeinit(struct TextShader *shader)
{
   glDeleteProgram(shader->hbShaderProgram);
}

void UploadTextShaderUniforms(struct TextShader *shader, struct TextShaderUniforms *uniforms)
{
   uint32_t program = shader->hbShaderProgram;
   _rendererUseShaderProgram(program);

   struct TextShaderUniformLocations *locations = &shader->uniformLocations;
   glUniformMatrix4fv(locations->matViewProjectionLoc, 1, GL_FALSE, uniforms->matViewProjection.col[0].raw);
   glUniform2f(locations->viewportLoc, (float) uniforms->viewport.raw[2], (float) uniforms->viewport.raw[3]);
   glUniform1f(locations->scaleLoc, (float) uniforms->scale);
   glUniform1f(locations->stemDarkeningLoc, uniforms->stemDarkening);
   glUniform1f(locations->debugLoc, uniforms->debug);
   glUniform1f(locations->gammaLoc, uniforms->gamma);
   glUniform1i(locations->hbGpuAtlasLoc, (int32_t) uniforms->hbGpuAtlas);
}

static uint32_t _createTextShader()
{
   const char *hbShaderVersion = "#version 330 core\n";
   const char *hbShaderPreamble = "#define HB_GPU_DEMO_DRAW\n";
   const char *hbVertexMain = ReadFileContents(ASSETS_DIR "shaders/harfbuzz.vert");
   const char *hbFragmentMain = ReadFileContents(ASSETS_DIR "shaders/harfbuzz.frag");

   uint32_t hbVertexShader;
   uint32_t hbFragmentShader;

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
   if (!ShaderGetCompileStatus(hbVertexShader))
      perror("vertex shader compilation failed");

   hbFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(hbFragmentShader, ArraySize(hbFragmentShaderSources), hbFragmentShaderSources, NULL);
   glCompileShader(hbFragmentShader);
   if (!ShaderGetCompileStatus(hbFragmentShader))
      perror("fragment shader compilation failed");

   uint32_t program = glCreateProgram();
   glAttachShader(program, hbVertexShader);
   glAttachShader(program, hbFragmentShader);
   glLinkProgram(program);
   if (!ShaderGetLinkStatus(program))
      perror("failed to link shader program");

   glDeleteShader(hbVertexShader);
   glDeleteShader(hbFragmentShader);
   free((void *) hbVertexMain);
   free((void *) hbFragmentMain);

   return program;
}

static void _rendererUseShaderProgram(uint32_t shaderProgram)
{
   assert(shaderProgram != 0);
   if (shaderProgram != CURRENT_SHADER_PROGRAM)
   {
      glUseProgram(shaderProgram);
      CURRENT_SHADER_PROGRAM = shaderProgram;
   }
}
