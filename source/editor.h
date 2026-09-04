#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#include "hb.h"
#include "hb-gpu.h"
#include "GLFW/glfw3.h"
#include "cglm/struct.h"
#include "unicode/unicode.h"

/**********
 * macros *
 *********/

#define ColorRGBHex(color)               \
   (((color >> 16) & 0xFF) / 255.0f),    \
       (((color >> 8) & 0xFF) / 255.0f), \
       (((color) & 0xFF) / 255.0f)

#define ColorRGBAHex(color)           \
   (((color >> 24) & 0xFF) / 255.0f), \
       ColorRGBHex(color)

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

/**********************************
 * user defined convenience types *
 *********************************/

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

/**!
 * note: todo: this should have the primitives related to a glyph,
 * like the foreground, the background etc... not the whole line..
 */
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
   u32 cursorOffsetBytes;
};

/*****************************************************************************
 * Opaque `Text` type. There would be a few implementations in the backend,  *
 * a `GapBuffer` implementation, a `Rope` implementation, and the user would *
 * be able to choose which implementation they want to use.                  *
 ****************************************************************************/
struct Text;

struct Editor
{
   /***************************
    * editor - internal state *
    **************************/
   /* todo: move out of editor */
   u32 xScrollOffset;
   bool lineDirty;

   /* note: add text here temporarily to access in window */
   struct Text *text;

   /***********************
    * editor - core state *
    **********************/
   f32 fontSize;
   char *fontFilePath;

   f64 lastTime;
   f64 timeDelta;
};

struct LineGlyphInfo
{
   struct GlyphInfo *glyphInfo;
   u32 glyphCount;

   /* note: naive implementation for now */
   u32 cursorLine;
   u32 cursorColumn;
};

struct Font
{
   char *fontPath;

   /******************************
    * font objects & the encoder *
    *****************************/
   hb_face_t *hbFace;
   hb_font_t *hbFont;
   hb_gpu_draw_t *hbDraw;

   /****************
    * font metrics *
    ***************/
   i32 hbAscent;
   i32 hbDescent;
   i32 hbMaxHeight;

   struct GlyphInfo *glyphCache;
};

/**!
 * Font Layout doesn't need to even have the Font with it. Font
 * is just a brush and it's the FontManager (in fontmanager.c)
 * readily provides one for each font.
 */
struct LineLayout
{
   struct GlyphVertex *glyphQuadVertices;
   u32 glyphQuadVerticesCount;
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

/* note: todo: Is a renderer supposed to be stateless, if so
 * then this LineRenderer naming is wrong*/
struct LineRenderer
{
   /**!
    * Uniforms of the line, like the position from where we start
    * drawing, the MVP matrix, the scale, gpu atlas, so on..
    */
   struct LineShaderUniforms uniforms;

   /**!
    * These are the primitives of a line, glyph quads mostly. These
    * do not change unless the line is edited.
    */

   struct
   {
      u32 vao;
      u32 vbo;
      u32 count;
      bool uploaded;
   } primitives;
};

/************
 * editor.c *
 ***********/

void editorInit(struct Editor *editor);
void editorCalcFrameTime(struct Editor *editor);
void editorDeInit(struct Editor *editor);

/*****************
 * fontmanager.c *
 ****************/

void fontManagerInit(char *editorFontPath);
void fontManagerDeInit();
void fontManagerMakeLineGlyphInfoSpec(struct LineGlyphInfo *lineGlyphInfo, char *lineUTF8, u64 lineByteLen);
struct GlyphAtlas *fontManagerGetGlyphAtlas();
struct Font *fontManagerGetFont(const char *filePath);
struct Font *fontManagerGetDefaultFont();
struct Font *fontManagerGetFontWithRune(rune codepoint);

void fontInit(struct Font *font, const char *filePath);
void fontDeInit(struct Font *font);

/************
 * layout.c *
 ***********/

void lineLayoutInit(struct LineLayout *layout);
void lineLayoutGlyphQuadsFromInfo(struct LineLayout *layout, struct LineGlyphInfo *lineGlyphInfo);
void lineLayoutDeInit(struct LineLayout *layout);

/**************
 * renderer.c *
 *************/

/**!
 * note:
 * The line renderer as of now contains one line's primitives. That's
 * fine, but one might want to do it on a per buffer basis, atleast
 * on the renderer side.
 *
 * Uploading just a few quads is not that expensive.. but that's something
 * to test.. just a note for later.. But there are both aspects.. With
 * these being for the lines, multiple buffers (splits etc) can eassentially
 * share the lines (unchanged) (with some kind of recounting)..
 */
void lineRendererInit(struct LineRenderer *renderer, struct LineShader *shader);
void lineRendererDeInit(struct LineRenderer *renderer);
void lineRendererRenderLine(struct LineRenderer *renderer, struct LineShader *shader);

void lineShaderInit(struct LineShader *shader);
void lineShaderDeInit(struct LineShader *shader);
void lineShaderCacheUniformLocations(struct LineShader *shader);

void lineShaderUploadUniforms(struct LineShader *shader, struct LineShaderUniforms *uniforms);

/**********
 * text.c *
 *********/

struct Text *textLoadFromFile(const char *filepath);
struct Text *textLoadFromData(const char *data, u32 dataLength);
bool textWriteToFile(const char *filepath);
u32 textGetLineCount(struct Text *text);
char *textGetUTF8Line(struct Text *text, u32 line);
void textDestroy(struct Text *text);

/* note: not thought through.. just trying to make some things work
 * this should be thought through again.. */

/* also note: that these functions for now just use strlen
 * to find the number of characters... that's not right,
 * we should use utf8 wrappers around the text, a char
 * is not a character.. */
bool textMoveCursorUp(struct Text *text);
bool textMoveCursorDown(struct Text *text);
bool textMoveCursorLeft(struct Text *text);
bool textMoveCursorRight(struct Text *text);
/**!
 * note: not sure if we should keep the cursor
 * info in the text.. maybe when we have a buffer
 * type, we can store the cursor location there..
 * that seems more appropriate, but for now let's keep it here..
 */
u32 textGetCursorLine(struct Text *text);
u32 textGetCursorColumn(struct Text *text);

/****************
 * filesystem.c *
 ***************/

char *readFileContents(const char *filPath);

/************
 * shader.c *
 ***********/

bool shaderGetCompileStatus(u32 shaderObject);
bool shaderGetLinkStatus(u32 shaderProgram);

/************
 * window.c *
 ***********/

GLFWwindow *windowCreate();
void windowDestroy(GLFWwindow *window);
void windowSetUserDataPtr(GLFWwindow *window, void *userData);
void mouseScroll(GLFWwindow *window, f64 x, f64 y);
void windowResize(GLFWwindow *window, i32 width, i32 height);
void mouseMove(GLFWwindow *window, f64 x, f64 y);
void keyPress(GLFWwindow *window, int key, int scancode, int action, int mods);

/***********
 * utils.c *
 **********/

/**!
 * Duplicates the string, i.e. allocates memory for the bytes and a `\0`,
 * and then uses `strcpy` to copy the string to the allocated memory.
 *
 * The caller is responsible for managing the `lifetime` of the returned
 * string i.e. freeing it. Returns `NULL` on error.
 */
char *stringDuplicate(const char *str);

#endif
