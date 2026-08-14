#include <stdlib.h>
#include <string.h>

#include "editor.h"

void fontLayoutInit(struct FontLayout *layout)
{
   /*********************************************************************************************
    * layout data - internal copy - we relayout when it is invalidated & sync with the renderer *
    ********************************************************************************************/
   layout->glyphQuadVertices      = NULL;
   layout->glyphQuadVerticesCount = 0;
}

void fontLayoutDeInit(struct FontLayout *layout)
{
   free(layout->glyphQuadVertices);
}
