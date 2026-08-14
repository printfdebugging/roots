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

typedef uint_least8_t u8;
typedef uint_least16_t u16;
typedef uint_least32_t u32;
typedef uint_least64_t u64;

typedef int_least8_t i8;
typedef int_least16_t i16;
typedef int_least32_t i32;
typedef int_least64_t i64;

typedef float f32;
typedef double f64;
typedef bool b8;
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
   u32 runeIdx;
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

/*****************************************************************************
 * Opaque `Text` type. There would be a few implementations in the backend,  *
 * a `GapBuffer` implementation, a `Rope` implementation, and the user would *
 * be able to choose which implementation they want to use.                  *
 ****************************************************************************/
struct Text;

struct Editor
{
   /*****************************************
    * editor - internal text representation *
    ****************************************/
   u32 lineBytelen;
   byte *lineBytes;

   /***************************
    * editor - internal state *
    **************************/
   i32 xScrollOffset;
   i32 cursorCol;
   i32 cursorOffset;

   /***********************
    * editor - core state *
    **********************/
   f32 fontSize;
   char *fontFilePath;

   /******************
    * window globals *
    *****************/
   GLFWwindow *window;
   i32 windowWidth;
   i32 windowHeight;
   f32 lastTime;
   f32 timeDelta;

   struct FontLayout *fontLayout;
   struct FontRenderer *fontRenderer;
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
struct FontLayout
{
   struct GlyphVertex *glyphQuadVertices;
   u32 glyphQuadVerticesCount;
};

struct FontRenderer
{
   /*************************************
    * renderer - draw uniform locations *
    ************************************/
   u32 hbShaderProgram;
   i32 matViewProjectionLoc;
   i32 viewportLoc;
   f32 scaleLoc;
   i32 positionLoc;
   i32 hbGpuAtlasLoc;
   i32 gammaLoc;
   i32 foregroundLoc;
   i32 debugLoc;
   i32 stemDarkeningLoc;
   i32 runeIdxLoc;

   /**********************************
    * renderer - draw uniform states *
    *********************************/
   mat4s matViewProjection;
   ivec4s viewport;
   f32 scale;
   vec2s position;
   i32 hbGpuAtlas;
   f32 gamma;
   vec4s foreground;
   b8 debug;
   b8 stemDarkening;
   i32 runeIdx;

   /*********************************
    * renderer - object store/cache *
    ********************************/
   u32 atlasTexture;
   u32 atlasTextureUnit;
   u32 atlasTextureBufferObject;
   u32 atlasCapacityBytes;
   u32 atlasCursorOffsetBytes;

   /**************************
    * renderer - layout data *
    *************************/
   u32 glyphQuadVerticesVAO;
   u32 glyphQuadVerticesVBO;
   b32 glyphQuadsUploaded;
};

/************
 * editor.c *
 ***********/

void editorInit(struct Editor *editor);
void editorDeInit(struct Editor *editor);

/*****************
 * fontmanager.c *
 ****************/

void fontManagerInit();
void fontManagerDeInit();
struct Font *fontManagerGetFont(const char *filePath);
struct Font *fontManagerGetFontWithRune(rune codepoint);

void fontInit(struct Font *font, const char *filePath);
void fontDeInit(struct Font *font);

/************
 * layout.c *
 ***********/

void fontLayoutInit(struct FontLayout *layout, const char *fontPath);
void fontLayoutDeInit(struct FontLayout *layout);

/**************
 * renderer.c *
 *************/

void fontRendererInit(struct FontRenderer *renderer);
void fontRendererDeInit(struct FontRenderer *renderer);
void fontRendererCacheUniformLoc(struct FontRenderer *renderer);
void fontRendererUploadUniforms(struct FontRenderer *renderer);
void fontRendererCreateShader(struct FontRenderer *renderer);
void fontRendererSetupAttribLocations(struct FontRenderer *renderer);

/**********
 * text.c *
 *********/

struct Text *textLoadFromFile(const char *filepath);
bool textWriteToFile(const char *filepath);
u32 textGetLineCount(struct Text *text);
char *textGetUTF8Line(struct Text *text, u32 line);
void textDestroy(struct Text *text);

/****************
 * filesystem.c *
 ***************/

char *readFileContents(const char *filPath);

/************
 * shader.c *
 ***********/

b8 shaderGetCompileStatus(u32 shaderObject);
b8 shaderGetLinkStatus(u32 shaderProgram);

/************
 * window.c *
 ***********/

void windowInit(struct Editor *editor);
void windowDeInit(struct Editor *editor);
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
