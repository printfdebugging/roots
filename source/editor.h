#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#include "hb.h"
#include "hb-gpu.h"
#include "GLFW/glfw3.h"
#include "cglm/struct.h"
#include "unicode/unicode.h"

/* function-like macros */

#define ColorRGBHex(color)               \
   (((color >> 16) & 0xFF) / 255.0f),    \
       (((color >> 8) & 0xFF) / 255.0f), \
       (((color) & 0xFF) / 255.0f)

#define ColorRGBAHex(color)           \
   (((color >> 24) & 0xFF) / 255.0f), \
       ColorRGBHex(color)

/* object-like macros */

#define U64_MAX 18446744073709551615UL
#define U32_MAX 4294967295U
#define U16_MAX 65535U
#define U8_MAX  255U
#define U64_MIN 0UL
#define U32_MIN 0U
#define U16_MIN 0U
#define U8_MIN  0U

#define I8_MAX  127
#define I16_MAX 32767
#define I32_MAX 2147483647
#define I64_MAX 9223372036854775807L
#define I8_MIN  (-I8_MAX - 1)
#define I16_MIN (-I16_MAX - 1)
#define I32_MIN (-I32_MAX - 1)
#define I64_MIN (-I64_MAX - 1)

#define KB (1024)
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)

#define MM   (1.0f)
#define CM   (10.0f * MM)
#define INCH (25.4f * MM)

#define TEXEL_SIZE        8
#define ATLAS_PAGE_SIZE   (TEXEL_SIZE * MB)
#define MAX_TEXTURE_COUNT 16

#define ArraySize(t) (sizeof(t) / sizeof(*t))

#define DEFAULT_FONT_FILE_PATH ASSETS_DIR "LilexNerdFont-Regular.ttf"
#define DEFAULT_FONT_SIZE      24

/* type aliases */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;
typedef int b32;

struct GlyphVertex
{
   f32 x;
   f32 y;
   f32 tx;
   f32 ty;
   f32 nx;
   f32 ny;
   f32 emPerPos;
   u32 atlasOffset;
   u32 hasCursor;
};

struct Extents
{
   f64 xMin;
   f64 yMin;
   f64 xMax;
   f64 yMax;
};

struct Point
{
   f64 x;
   f64 y;
};

struct GlyphInfo
{
   f64 advance;
   i32 upem;
   u32 atlasOffset;
   b32 empty;
   b32 cached;
   struct Extents extents;
};

struct GlyphAtlas
{
   u32 texture;
   i32 textureUnit;
   u32 textureBufferObject;
   u32 capacityBytes;

   /**!
    * note: This doesn't start at 0, but at `TEXEL_SIZE`. Empty glyphs
    * don't have any glyph data, so their `GlyphInfo.atlasOffset` is set
    * to 0, and if we start at 0 here, that would then use the first uploaded
    * glyph for the spaces..
    */
   u32 cursorOffsetBytes;
};

/**!
 * Opaque `Text` type. There would be a few implementations in the backend,
 * a `GapBuffer` implementation, a `Rope` implementation, and the user would
 * be able to choose which implementation they want to use.
 */
struct Text;

struct Buffer
{
   u32 windowId;
   u32 rendererId;
   u32 textId;

   /* Buffer specific state. */

   /**!
    * These are not to be overwritten directly. Only
    * return values from a Text object's API calls should
    * be assifned to these.
    */
   u32 cursorLine;
   u32 cursorColumn;

   /**!
    * The horizontal and vertical scroll offsets of a buffer.
    * These are used to construct the MVP matrix for the buffer.
    */
   u32 hOffset;
   u32 vOffset;

   /**!
    * The `Editor` holds the objects and the buffers just have
    * the IDs of their objects in Editor's arrays.
    */
   struct Editor *editor;
};

struct GLFWwindowOptions
{
   bool visible;
   bool transparent;
   i32 width;
   i32 height;
   const char *title;
   GLFWwindow *shared;
   void *userdata;

   GLFWframebuffersizefun fbResizeFn;
   GLFWscrollfun scrollFn;
   GLFWcursorposfun curPosFn;
   GLFWkeyfun keyFn;
};

struct Editor
{
   /* arrays */
   struct Text **text;
   struct GLFWwindow **window;
   struct LineRenderer **lineRenderer;
   struct LineShader *lineShader; /* shared among Buffer objects */
   struct Buffer **textBuffer;    /* just a bunch of indices into Editor's object arrays */

   /* counts */
   u32 textCount;
   u32 lineRendererCount;
   u32 windowCount;
   u32 bufferCount;

   /* config */
   f32 fontSize;
   char *fontFilePath;
   bool initialized;

   /* frame book-keeping */
   f64 lastTime;
   f64 timeDelta;
};

struct Font
{
   char *fontPath;

   /* font objects & the encoder */
   hb_face_t *hbFace;
   hb_font_t *hbFont;
   hb_gpu_draw_t *hbDraw;

   /* font metrics */
   i32 hbAscent;
   i32 hbDescent;
   i32 hbMaxHeight;

   struct GlyphInfo *glyphCache;
};

struct LineShaderUniforms
{
   mat4s matViewProjection;
   ivec4s viewport;
   f32 scale;
   vec2s position;
   i32 hbGpuAtlas;
   f32 gamma;
   vec4s foreground;
   bool debug;
   bool stemDarkening;
};

struct LineShaderUniformLocations
{
   i32 matViewProjectionLoc;
   i32 viewportLoc;
   i32 scaleLoc;
   i32 positionLoc;
   i32 hbGpuAtlasLoc;
   i32 gammaLoc;
   i32 foregroundLoc;
   i32 debugLoc;
   i32 stemDarkeningLoc;
};

/**!
 * A Line shader is shared between various line renderers. This
 * does not contain any state, but allows one to quickly set
 * the state using `LineShaderUniforms` and draw/redraw a line..
 */
struct LineShader
{
   u32 hbShaderProgram;
   struct LineShaderUniformLocations uniformLocations;
};

struct LineRenderer
{
   /**!
    * Uniforms of the line, like the position from where we start
    * drawing, the MVP matrix, the scale, gpu atlas, so on..
    */
   struct LineShaderUniforms uniforms;

   /**!
    * The vbo data, kept for compuation on the CPU, like the
    * hit-test, scrolling etc.
    */
   struct GlyphVertex *vertices;

   /* OpenGL primitives */
   u32 vao;
   u32 vbo;
   u32 count;
   bool uploaded;
};

/* editor.c */
bool editorInit(struct Editor *editor);
void editorCalcFrameTime(struct Editor *editor);
bool editorRun(struct Editor *editor);
bool editorDeInit(struct Editor *editor);
i32 editorLoadTextFile(struct Editor *editor, const char *filePath);
i32 editorCreateWindow(struct Editor *editor, struct GLFWwindowOptions opts);

/* fontmanager.c */
void fontManagerInit(char *editorFontPath);
void fontManagerDeInit();
void fontManagerLayoutLine(struct LineRenderer *renderer, char *lineUTF8, u64 lineByteLen);
struct GlyphAtlas *fontManagerGetGlyphAtlas();
struct Font *fontManagerGetFont(const char *filePath);
struct Font *fontManagerGetDefaultFont();
struct Font *fontManagerGetFontWithRune(rune codepoint);
void fontInit(struct Font *font, const char *filePath);
void fontDeInit(struct Font *font);

/* renderer.c */
void lineRendererInit(struct LineRenderer *renderer, struct LineShader *shader);
void lineRendererDeInit(struct LineRenderer *renderer);
void lineRendererRenderLine(struct LineRenderer *renderer, struct LineShader *shader);
void lineShaderInit(struct LineShader *shader);
void lineShaderDeInit(struct LineShader *shader);
void lineShaderUploadUniforms(struct LineShader *shader, struct LineShaderUniforms *uniforms);

/* text.c */
struct Text *textLoadFromFile(const char *filepath);
struct Text *textLoadFromData(const char *data, u32 dataLength);
bool textWriteToFile(const char *filepath);
u32 textGetLineCount(struct Text *text);
char *textGetUTF8Line(struct Text *text, u32 line);
void textDestroy(struct Text *text);
bool textMoveCursorUp(struct Text *text);
bool textMoveCursorDown(struct Text *text);
bool textMoveCursorLeft(struct Text *text);
bool textMoveCursorRight(struct Text *text);
u32 textGetCursorLine(struct Text *text);
u32 textGetCursorColumn(struct Text *text);

/* filesystem.c */
char *readFileContents(const char *filPath);

/* shader.c */
bool shaderGetCompileStatus(u32 shaderObject);
bool shaderGetLinkStatus(u32 shaderProgram);

/* window.c */
GLFWwindow *windowCreate(struct GLFWwindowOptions opts);
void windowDestroy(GLFWwindow *window);
void windowSetUserDataPtr(GLFWwindow *window, void *userData);
void mouseScroll(GLFWwindow *window, f64 x, f64 y);
void windowResize(GLFWwindow *window, i32 width, i32 height);
void mouseMove(GLFWwindow *window, f64 x, f64 y);
void keyPress(GLFWwindow *window, int key, int scancode, int action, int mods);

/* utils.c */
char *stringDuplicate(const char *str);

#endif
