#include <stdlib.h>

#include "editor.h"

void layoutInit(struct Layout *layout)
{
   /*******************************************
    * layout - objects which do the layouting *
    ******************************************/
   layout->hbFace = NULL;
   layout->hbFont = NULL;
   layout->hbDraw = NULL;

   /*********************************************************************************************
    * layout data - internal copy - we relayout when it is invalidated & sync with the renderer *
    ********************************************************************************************/
   layout->glyphQuadVertices      = NULL;
   layout->glyphQuadVerticesCount = 0;
   layout->glyphCache             = NULL;
}

void layoutDeInit(struct Layout *layout)
{
   hb_face_destroy(layout->hbFace);
   hb_font_destroy(layout->hbFont);
   hb_gpu_draw_destroy(layout->hbDraw);

   free(layout->glyphQuadVertices);
}
