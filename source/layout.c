#include <stdlib.h>
#include <string.h>

#include "editor.h"

void layoutInit(struct Layout *layout, const char *fontPath)
{
   /*******************************************
    * layout - objects which do the layouting *
    ******************************************/
   layout->hbFace = NULL;
   layout->hbFont = NULL;
   layout->hbDraw = NULL;

   i32 fontPathLen = strlen(fontPath);
   assert(fontPathLen != 0);
   layout->fontPath = calloc(fontPathLen + 1, sizeof(char));
   strcpy(layout->fontPath, fontPath);

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
   free(layout->fontPath);
}
