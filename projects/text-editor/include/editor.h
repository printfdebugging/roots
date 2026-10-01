#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>
#include <assert.h>

#include "GLFW/glfw3.h"
#include "cglm/struct.h"
#include "unicode/unicode.h"

#include "font-manager/font-manager.h"

#include "text.h"

#define DEFAULT_FONT_FILE_PATH ASSETS_DIR "fonts/LilexNerdFont-Regular.ttf"
#define DEFAULT_FONT_SIZE      34
#define DEFAULT_WINDOW_ICON    ASSETS_DIR "images/icon.png"
#define TABSTOP                3

#define INVALID_ID -1

#define NUL             0x00
#define SPACE           0x20
#define NEWLINE         0x0a
#define HORIZONTAL_TAB  0x09
#define CARRIAGE_RETURN 0x0d

struct Point
{
   double x;
   double y;
};

/**!
 * @brief Integers are used intentionally here. We won't be doing floating
 * point32_t anything when it comes to layouting etc. We just don't want that
 * hassle. So we would do all the layouting in screen space coordinates.
 */
struct Rectangle
{
   int64_t x;
   int64_t y;
   int64_t w;
   int64_t h;
};

/**!
 * Opaque `Text` type. There would be a few implementations in the backend,
 * a `GapBuffer` implementation, a `Rope` implementation, and the user would
 * be able to choose which implementation they want to use.
 */
struct Text;

/**!
 * @brief Usually it's said that buffers are shared between frames, but the
 * sharing is in terms of attributes, like two buffers might share a frame,
 * or two buffers might not share a frame but share the text they are showing.
 *
 * This does not mean that they are the same buffers. They can have different
 * cursor positions, different state that they are rendering, different visibility.
 * So they are not at all the same thing. Don't tell the user about this ;).
 */

/*
 * struct Buffer
 * {
 *    int32_t winId;
 *    int32_t txtId;
 * };
 */

/* this is layout's job not vislinerenderer's
 * so first step is to split line renderers from layouting :) again*/
//};

struct GLFWwindowOptions
{
   bool visible;
   bool transparent;
   int32_t width;
   int32_t height;
   const char *title;
   const char *icon;
   int32_t sharedWinId;

   GLFWframebuffersizefun fbResizeFn;
   GLFWscrollfun scrollFn;
   GLFWcursorposfun curPosFn;
   GLFWkeyfun keyFn;
};

/**!
 * Font manager is a subsystem we request for the font objects.
 * This way, we don't have to manage the lifetime of these objects. And
 * since these objects are shared, so is the glyphCache.
 */

/* layout space constants */
#define CHARS       60
#define LINES       20
#define VERTICES    6
#define VERTEX_SIZE sizeof(struct GlyphVertex)

#ifdef LOGGING
#define LOG_INFO(...)  fprintf(stderr, __VA_ARGS__);
#define LOG_EVENT(...) fprintf(stderr, __VA_ARGS__);
#else
#define LOG_INFO(...)
#define LOG_EVENT(...)
#endif

struct LayoutMap
{
   uint32_t textLineIdx;
   bool layouted;
   bool uploaded;
};

struct Editor
{
   /* arrays */
   struct Text *text;
   struct GLFWwindow *window;
   struct BufferRenderer *bufRenderer;
   struct TextShader *lineShader; /* shared among Buffer objects */

   struct GlyphVertex *vertices;
   /* this is fixed by the constants above */
   uint32_t verticesCount;
   struct LayoutMap layoutMap[LINES];

   /*
    * This is a baton which the update -> layout -> upload stages
    * pass to each other to signal if things changed and whether
    * they need to do something about it.
    *
    * The idea is that each stage just checks "do i need to do anything"
    * and only when the answer is "yes", should they go out and look
    * into the layoutMap about what changed.
    *
    * - todo: add LayoutType - CURSOR_MOVE, SCROLL, RESIZE..
    * - todo: add UploadType - MORPH, BUFFER_SUBDATA
    */
   struct
   {
      bool updated;
      bool layouted;
      bool uploaded;
   } layoutMapState;

   /*
    * The cursor does not exist for the text, it's just a marker the
    * user has (in the buffer) to say "make edits here" etc.
    */
   uint32_t cursorLine;
   uint32_t cursorColumn;

   uint32_t lineOffset;
   uint32_t columnOffset;

   /* config */
   float fontSize;
   char *fontFilePath;
   bool initialized;

   /* frame book-keeping */
   double tLast;
   double tDelta;
};

struct TextShaderUniforms
{
   mat4s matViewProjection;
   ivec4s viewport;
   float scale;
   int32_t hbGpuAtlas;
   float gamma;
   bool debug;
   bool stemDarkening;
};

struct TextShaderUniformLocations
{
   int32_t matViewProjectionLoc;
   int32_t viewportLoc;
   int32_t scaleLoc;
   int32_t positionLoc;
   int32_t hbGpuAtlasLoc;
   int32_t gammaLoc;
   int32_t foregroundLoc;
   int32_t debugLoc;
   int32_t stemDarkeningLoc;
};

/**!
 * A Line shader is shared between various line renderers. This
 * does not contain any state, but allows one to quickly set
 * the state using `TextShaderUniforms` and draw/redraw a line..
 */
struct TextShader
{
   uint32_t hbShaderProgram;
   struct TextShaderUniformLocations uniformLocations;
};

/* for now BufferRender and LineLayout don't know about each other, that's fine. */
struct BufferRenderer
{
   /**!
    * Uniforms of the line, like the position from where we start
    * drawing, the MVP matrix, the scale, gpu atlas, so on..
    */
   struct TextShaderUniforms uniforms;

   /* OpenGL primitives */
   uint32_t vao;
   uint32_t vbo;

   /* note: this is inconsiquencial in layouting, considering that we
    * are going for a fixed buffer approach for now */
   uint32_t count;
   bool uploaded;
};

struct LineLayout
{
   /**!
    * The vbo data, kept for compuation on the CPU, like the
    * hit-test, scrolling etc.
    */
   struct GlyphVertex *vertices;

   /* this lives here for now, but not for long,
    * we would have a separate array for these.. */
   bool dirty;
   uint32_t count;
};

/* editor.c */

enum UpdateEvent
{
   EDITOR_STARTUP,
   KEY_PRESS,
};

union UpdateState
{
   int32_t glfwKey;
};

/**!
 * These are the core editor functions, so they can access the editor
 * directly.
 */
bool Init();
void CalcFrameTime();
bool Run();
bool ShouldClose();
bool DeInit();
void Update(enum UpdateEvent event, union UpdateState state);
void Layout();
void Upload();
void Render();

void LayoutBuffer();
void UploadBuffer();
void RenderBuffer();

bool CreateGLFWwindow(struct GLFWwindowOptions opts);
struct Rectangle GetWindowBounds();
void SwapGLBuffers();
bool LoadTextFile(const char *filePath);
void EditorOpenFile(const char *path);

void CreateTextShader(struct TextShader *shader);
void DestroyTextShader(struct TextShader *shader);
void UploadTextShaderUniforms(struct TextShader *shader, struct TextShaderUniforms *uniforms);

void scrollFn(GLFWwindow *window, double x, double y);
void fbResizeFn(GLFWwindow *window, int32_t width, int32_t height);
void curPosFn(GLFWwindow *window, double x, double y);
void keyFn(GLFWwindow *window, int32_t key, int32_t scancode, int32_t action, int32_t mods);

/* renderer.c */
void InitBufferRenderer(struct BufferRenderer *renderer, struct TextShader *shader);
void DeInitBufferRenderer(struct BufferRenderer *renderer);

#endif
