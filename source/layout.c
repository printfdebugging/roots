#include <stdlib.h>
#include <string.h>

#include "editor.h"

void lineLayoutInit(struct LineLayout *layout)
{
   /*********************************************************************************************
    * layout data - internal copy - we relayout when it is invalidated & sync with the renderer *
    ********************************************************************************************/
   layout->glyphQuadVertices      = NULL;
   layout->glyphQuadVerticesCount = 0;
}

/**!
 * note: don't realayout the whole line, just the visible part + some more
 * todo:
 */
void lineLayoutGlyphQuadsFromInfo(struct LineLayout *layout, struct LineGlyphInfo *lineGlyphInfo)
{
   layout->glyphQuadVerticesCount = lineGlyphInfo->glyphCount * 6;
   layout->glyphQuadVertices      = realloc(layout->glyphQuadVertices, layout->glyphQuadVerticesCount * sizeof(struct GlyphVertex));

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

      layout->glyphQuadVertices[glyphQuadOffset + 0] = glyphQuadCorners[0];
      layout->glyphQuadVertices[glyphQuadOffset + 1] = glyphQuadCorners[1];
      layout->glyphQuadVertices[glyphQuadOffset + 2] = glyphQuadCorners[2];
      layout->glyphQuadVertices[glyphQuadOffset + 3] = glyphQuadCorners[1];
      layout->glyphQuadVertices[glyphQuadOffset + 4] = glyphQuadCorners[2];
      layout->glyphQuadVertices[glyphQuadOffset + 5] = glyphQuadCorners[3];

      glyphPosition.x += glyphInfo->extents.xMax;
      glyphPosition.y += 0;
   }
}

void lineLayoutDeInit(struct LineLayout *layout)
{
   free(layout->glyphQuadVertices);
}
