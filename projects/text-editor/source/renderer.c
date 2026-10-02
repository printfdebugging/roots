#include "editor.h"

#include "utils/macros.h"
#include "utils/string.h"
#include "utils/shader.h"

#include "glad/glad.h"

static uint32_t CURRENT_SHADER_PROGRAM = 0;

static uint32_t _create_text_shader();
static void _renderer_use_shader_program(uint32_t program);

/* this has to now happen once for each BufferRenderer (doens't exist yet.) */
/* todo: warning: refactor it asap */
void buffer_renderer_init(struct buffer_renderer *renderer, struct text_shader *shader)
{
   /* uniforms */
   renderer->uniforms = (struct text_shader_uniforms) {
      .mvp = (mat4s) { GLM_MAT4_IDENTITY_INIT },
      .viewport = GLMS_IVEC4_ZERO,
      .scale = 0,
      .hb_gpu_atlas = 0,
      .gamma = 0,
      .debug = false,
      .stem_darkening = false,
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

   uint32_t program = shader->hb_shader_program;
   int32_t attribute_location = -1;
   int32_t glyph_quad_object_stride = sizeof(struct glyph_vertex);

   /**!
    * warning: These should be called only after the buffer object
    * is bound to the array buffer, otherwise it can lead to bugs like all
    * the vertex array buffers using the same vertex buffer object which
    * was set when this function was called.
    */

   attribute_location = glGetAttribLocation(program, "a_position");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribPointer((uint32_t) attribute_location, 2, GL_FLOAT, GL_FALSE, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, x));

   attribute_location = glGetAttribLocation(program, "a_texcoord");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribPointer((uint32_t) attribute_location, 2, GL_FLOAT, GL_FALSE, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, tx));

   attribute_location = glGetAttribLocation(program, "a_normal");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribPointer((uint32_t) attribute_location, 2, GL_FLOAT, GL_FALSE, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, nx));

   attribute_location = glGetAttribLocation(program, "a_emPerPos");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribPointer((uint32_t) attribute_location, 1, GL_FLOAT, GL_FALSE, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, epp));

   attribute_location = glGetAttribLocation(program, "a_glyphLoc");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribIPointer((uint32_t) attribute_location, 1, GL_UNSIGNED_INT, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, atlas_offset));

   attribute_location = glGetAttribLocation(program, "a_hasCursor");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribIPointer((uint32_t) attribute_location, 1, GL_UNSIGNED_INT, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, has_cursor));

   attribute_location = glGetAttribLocation(program, "a_fgColor");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribIPointer((uint32_t) attribute_location, 4, GL_UNSIGNED_INT, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, fg_color));

   attribute_location = glGetAttribLocation(program, "a_bgColor");
   glEnableVertexAttribArray((uint32_t) attribute_location);
   glVertexAttribIPointer((uint32_t) attribute_location, 4, GL_UNSIGNED_INT, glyph_quad_object_stride, (const void *) offsetof(struct glyph_vertex, bg_color));
}

/* todo: this also needs fixing asap */
void buffer_renderer_deinit(struct buffer_renderer *renderer)
{
   /* primitives */
   renderer->uploaded = false;
   renderer->count = 0;
   glDeleteBuffers(1, &renderer->vbo);
   glDeleteVertexArrays(1, &renderer->vao);
}

void text_shader_create(struct text_shader *shader)
{
   shader->hb_shader_program = _create_text_shader();
   shader->uniform_locations = (struct text_shader_uniform_locations) {
      .mvp = -1,
      .viewport = -1,
      .scale = -1,
      .position = -1,
      .hb_gpu_atlas = -1,
      .gamma = -1,
      .debug = -1,
      .stem_darkening = -1,
   };

   uint32_t program = shader->hb_shader_program;
   _renderer_use_shader_program(program);

   shader->uniform_locations = (struct text_shader_uniform_locations) {
      .mvp = glGetUniformLocation(program, "u_matViewProjection"),
      .viewport = glGetUniformLocation(program, "u_viewport"),
      .scale = glGetUniformLocation(program, "u_scale"),
      .position = glGetUniformLocation(program, "u_position"),
      .gamma = glGetUniformLocation(program, "u_gamma"),
      .debug = glGetUniformLocation(program, "u_debug"),
      .stem_darkening = glGetUniformLocation(program, "u_stem_darkening"),
      .hb_gpu_atlas = glGetUniformLocation(program, "hb_gpu_atlas"),
   };
}

void text_shader_destroy(struct text_shader *shader)
{
   glDeleteProgram(shader->hb_shader_program);
}

void text_shader_upload_uniforms(struct text_shader *shader, struct text_shader_uniforms *uniforms)
{
   uint32_t program = shader->hb_shader_program;
   _renderer_use_shader_program(program);

   struct text_shader_uniform_locations *locations = &shader->uniform_locations;
   glUniformMatrix4fv(locations->mvp, 1, GL_FALSE, uniforms->mvp.col[0].raw);
   glUniform2f(locations->viewport, (float) uniforms->viewport.raw[2], (float) uniforms->viewport.raw[3]);
   glUniform1f(locations->scale, (float) uniforms->scale);
   glUniform1f(locations->stem_darkening, uniforms->stem_darkening);
   glUniform1f(locations->debug, uniforms->debug);
   glUniform1f(locations->gamma, uniforms->gamma);
   glUniform1i(locations->hb_gpu_atlas, (int32_t) uniforms->hb_gpu_atlas);
}

static uint32_t _create_text_shader()
{
   const char *hb_shader_version = "#version 330 core\n";
   const char *hb_shader_preamble = "#define HB_GPU_DEMO_DRAW\n";
   const char *hb_vertex_main = string_read_file_contents(ASSETS_DIR "shaders/harfbuzz.vert");
   const char *hb_fragment_main = string_read_file_contents(ASSETS_DIR "shaders/harfbuzz.frag");

   uint32_t hb_vertex_shader;
   uint32_t hb_fragment_shader;

   const char *hb_vertex_shader_source[] = {
      hb_shader_version,
      hb_shader_preamble,
      hb_gpu_shader_source(HB_GPU_SHADER_STAGE_VERTEX, HB_GPU_SHADER_LANG_GLSL),
      hb_gpu_draw_shader_source(HB_GPU_SHADER_STAGE_VERTEX, HB_GPU_SHADER_LANG_GLSL),
      hb_vertex_main,
   };

   const char *hb_fragment_shader_source[] = {
      hb_shader_version,
      hb_shader_preamble,
      hb_gpu_shader_source(HB_GPU_SHADER_STAGE_FRAGMENT, HB_GPU_SHADER_LANG_GLSL),
      hb_gpu_draw_shader_source(HB_GPU_SHADER_STAGE_FRAGMENT, HB_GPU_SHADER_LANG_GLSL),
      hb_fragment_main,
   };

   hb_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(hb_vertex_shader, array_size(hb_vertex_shader_source), hb_vertex_shader_source, NULL);
   glCompileShader(hb_vertex_shader);
   if (!shader_get_compile_status(hb_vertex_shader))
      perror("vertex shader compilation failed");

   hb_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(hb_fragment_shader, array_size(hb_fragment_shader_source), hb_fragment_shader_source, NULL);
   glCompileShader(hb_fragment_shader);
   if (!shader_get_compile_status(hb_fragment_shader))
      perror("fragment shader compilation failed");

   uint32_t program = glCreateProgram();
   glAttachShader(program, hb_vertex_shader);
   glAttachShader(program, hb_fragment_shader);
   glLinkProgram(program);
   if (!shader_get_link_status(program))
      perror("failed to link shader program");

   glDeleteShader(hb_vertex_shader);
   glDeleteShader(hb_fragment_shader);
   free((void *) hb_vertex_main);
   free((void *) hb_fragment_main);

   return program;
}

static void _renderer_use_shader_program(uint32_t program)
{
   assert(program != 0);
   if (program != CURRENT_SHADER_PROGRAM)
   {
      glUseProgram(program);
      CURRENT_SHADER_PROGRAM = program;
   }
}
