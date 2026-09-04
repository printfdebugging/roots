#include "editor.h"

#include "glad/glad.h"

static u32 CURRENT_SHADER_PROGRAM = 0;

static u32 _lineShaderCreate();
static void _rendererUseShaderProgram(u32 shaderProgram);

void lineRendererInit(struct LineRenderer *renderer, struct LineShader *shader)
{
   /* layout */
   renderer->vertices = NULL;

   /* uniforms */
   renderer->uniforms = (struct LineShaderUniforms) {
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

   /* primitives */
   glGenVertexArrays(1, &renderer->vao);
   glGenBuffers(1, &renderer->vbo);
   renderer->count    = 0;
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

   u32 program               = shader->hbShaderProgram;
   i32 attribLocation        = -1;
   i32 glyphQuadObjectStride = sizeof(struct GlyphVertex);

   /**!
    * warning: These should be called only after the buffer object
    * is bound to the array buffer, otherwise it can lead to bugs like all
    * the vertex array buffers using the same vertex buffer object which
    * was set when this function was called.
    */

   attribLocation = glGetAttribLocation(program, "a_position");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, x));

   attribLocation = glGetAttribLocation(program, "a_texcoord");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, tx));

   attribLocation = glGetAttribLocation(program, "a_normal");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 2, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, nx));

   attribLocation = glGetAttribLocation(program, "a_emPerPos");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribPointer((u32) attribLocation, 1, GL_FLOAT, GL_FALSE, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, emPerPos));

   attribLocation = glGetAttribLocation(program, "a_glyphLoc");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, atlasOffset));

   attribLocation = glGetAttribLocation(program, "a_hasCursor");
   glEnableVertexAttribArray((u32) attribLocation);
   glVertexAttribIPointer((u32) attribLocation, 1, GL_UNSIGNED_INT, glyphQuadObjectStride, (const void *) offsetof(struct GlyphVertex, hasCursor));
}

void lineRendererDeInit(struct LineRenderer *renderer)
{
   /* layout */
   free(renderer->vertices);

   /* primitives */
   renderer->uploaded = false;
   renderer->count    = 0;
   glDeleteBuffers(1, &renderer->vbo);
   glDeleteVertexArrays(1, &renderer->vao);
}

/**!
 * note: don't realayout the whole line, just the visible part + some more
 */
void lineRendererGlyphQuadsFromInfo(struct LineRenderer *renderer, struct LineGlyphInfo *lineGlyphInfo)
{
   renderer->count    = lineGlyphInfo->glyphCount * 6;
   renderer->vertices = realloc(renderer->vertices, renderer->count * sizeof(struct GlyphVertex));

   struct Point glyphPosition = { .x = 0, .y = 0 };
   for (u32 glyphIdx = 0; glyphIdx < lineGlyphInfo->glyphCount; ++glyphIdx)
   {
      bool hasCursor              = lineGlyphInfo->cursorColumn == glyphIdx; /* naive approach */
      struct GlyphInfo *glyphInfo = &lineGlyphInfo->glyphInfo[glyphIdx];

      /**********************
       * create glyph quads *
       *********************/

      glyphPosition.x += glyphInfo->extents.xMin;
      glyphPosition.y += 0;

      struct GlyphVertex glyphQuadCorners[4];
      for (int cornerIdx = 0; cornerIdx < 4; cornerIdx++)
      {
         i32 cx = (cornerIdx >> 1) & 1;
         i32 cy = cornerIdx & 1;
         f64 ex = (1 - cx) * glyphInfo->extents.xMin + cx * glyphInfo->extents.xMax;
         f64 ey = (1 - cy) * glyphInfo->extents.yMin + cy * glyphInfo->extents.yMax;

         glyphQuadCorners[cornerIdx] = (struct GlyphVertex) {
            .x           = (f32) glyphPosition.x,
            .y           = (f32) glyphPosition.y,
            .tx          = (f32) ex,
            .ty          = (f32) ey,
            .nx          = cx ? 1.f : -1.f,
            .ny          = cy ? -1.f : 1.f,
            .emPerPos    = 1.0,
            .atlasOffset = glyphInfo->atlasOffset / TEXEL_SIZE,
            .hasCursor   = hasCursor,
         };
      }

      u32 glyphQuadOffset = glyphIdx * 6;

      /* this is hardly of any use to us. this goes to OpenGL, so we better not keep this form
       * untill we need to upload.. */
      renderer->vertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
      renderer->vertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
      renderer->vertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
      renderer->vertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
      renderer->vertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
      renderer->vertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

      glyphPosition.x += glyphInfo->extents.xMax;
      glyphPosition.y += 0;
   }

   glBindVertexArray(renderer->vao);
   glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(struct GlyphVertex) * renderer->count, renderer->vertices, GL_STATIC_DRAW);
   renderer->count    = renderer->count;
   renderer->uploaded = true;
}

void lineShaderInit(struct LineShader *shader)
{
   shader->hbShaderProgram  = _lineShaderCreate();
   shader->uniformLocations = (struct LineShaderUniformLocations) {
      .matViewProjectionLoc = -1,
      .viewportLoc          = -1,
      .scaleLoc             = -1,
      .positionLoc          = -1,
      .hbGpuAtlasLoc        = -1,
      .gammaLoc             = -1,
      .foregroundLoc        = -1,
      .debugLoc             = -1,
      .stemDarkeningLoc     = -1,
   };

   u32 program = shader->hbShaderProgram;
   _rendererUseShaderProgram(program);

   shader->uniformLocations = (struct LineShaderUniformLocations) {
      .matViewProjectionLoc = glGetUniformLocation(program, "u_matViewProjection"),
      .viewportLoc          = glGetUniformLocation(program, "u_viewport"),
      .scaleLoc             = glGetUniformLocation(program, "u_scale"),
      .positionLoc          = glGetUniformLocation(program, "u_position"),
      .gammaLoc             = glGetUniformLocation(program, "u_gamma"),
      .foregroundLoc        = glGetUniformLocation(program, "u_foreground"),
      .debugLoc             = glGetUniformLocation(program, "u_debug"),
      .stemDarkeningLoc     = glGetUniformLocation(program, "u_stem_darkening"),
      .hbGpuAtlasLoc        = glGetUniformLocation(program, "hb_gpu_atlas"),
   };
}

void lineShaderDeInit(struct LineShader *shader)
{
   glDeleteProgram(shader->hbShaderProgram);
}

void lineShaderDeinit(struct LineShader *shader)
{
   glDeleteProgram(shader->hbShaderProgram);
}

void lineShaderUploadUniforms(struct LineShader *shader, struct LineShaderUniforms *uniforms)
{
   u32 program = shader->hbShaderProgram;
   _rendererUseShaderProgram(program);

   struct LineShaderUniformLocations *locations = &shader->uniformLocations;
   glUniformMatrix4fv(locations->matViewProjectionLoc, 1, GL_FALSE, uniforms->matViewProjection.col[0].raw);
   glUniform4fv(locations->foregroundLoc, 1, uniforms->foreground.raw);
   glUniform2fv(locations->positionLoc, 1, uniforms->position.raw);
   glUniform2f(locations->viewportLoc, (f32) uniforms->viewport.raw[2], (f32) uniforms->viewport.raw[3]);
   glUniform1f(locations->scaleLoc, (f32) uniforms->scale);
   glUniform1f(locations->stemDarkeningLoc, uniforms->stemDarkening);
   glUniform1f(locations->debugLoc, uniforms->debug);
   glUniform1f(locations->gammaLoc, uniforms->gamma);
   glUniform1i(locations->hbGpuAtlasLoc, (i32) uniforms->hbGpuAtlas);
}

static u32 _lineShaderCreate()
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

   u32 program = glCreateProgram();
   glAttachShader(program, hbVertexShader);
   glAttachShader(program, hbFragmentShader);
   glLinkProgram(program);
   if (!shaderGetLinkStatus(program))
      perror("failed to link shader program");

   glDeleteShader(hbVertexShader);
   glDeleteShader(hbFragmentShader);
   free((void *) hbVertexMain);
   free((void *) hbFragmentMain);

   return program;
}

static void _rendererUseShaderProgram(u32 shaderProgram)
{
   assert(shaderProgram != 0);
   if (shaderProgram != CURRENT_SHADER_PROGRAM)
   {
      glUseProgram(shaderProgram);
      CURRENT_SHADER_PROGRAM = shaderProgram;
   }
}
