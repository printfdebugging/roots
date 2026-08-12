#ifndef LAYOUT_H
#define LAYOUT_H

#include "hb.h"
#include "hb-gpu.h"
#include "hb-ot.h"

#include "types.h"

struct Layout
{
   /*******************************************
    * layout - objects which do the layouting *
    ******************************************/
   hb_face_t *hbFace;
   hb_font_t *hbFont;
   hb_gpu_draw_t *hbDraw;

   /*********************************************************************************************
    * layout data - internal copy - we relayout when it is invalidated & sync with the renderer *
    ********************************************************************************************/
   struct GlyphVertex *glyphQuadVertices;
   u32 glyphQuadVerticesCount;
   struct GlyphInfo *glyphCache;
};

void layoutInit(struct Layout *layout);
void layoutDeInit(struct Layout *layout);

#endif
