#include <stdlib.h>
#include <string.h>

#include "hb-ot.h"

#include "editor.h"

void _layoutInitHarfbuzz(struct Layout *layout)
{
   /*********************************************************
    * harfbuzz: font loading & shape encoder initialization *
    ********************************************************/

   hb_blob_t *hbBlob = NULL;
   if (!(hbBlob = hb_blob_create_from_file(layout->fontPath)) ||
       !(layout->hbFace = hb_face_create(hbBlob, 0)) ||
       !(layout->hbFont = hb_font_create(layout->hbFace)) ||
       !(layout->hbDraw = hb_gpu_draw_create_or_fail()))
   {
      perror("failed to initialize harfbuzz");
   }

   hb_blob_destroy(hbBlob);
}

void _layoutLoadFontMetrics(struct Layout *layout)
{
   const hb_ot_metrics_tag_t ASCENT_HHEA  = HB_TAG('H', 'a', 's', 'c');
   const hb_ot_metrics_tag_t DESCENT_HHEA = HB_TAG('H', 'd', 's', 'c');

   hb_ot_metrics_get_position(layout->hbFont, ASCENT_HHEA, &layout->hbAscent);
   hb_ot_metrics_get_position(layout->hbFont, DESCENT_HHEA, &layout->hbDescent);
   hb_ot_metrics_get_position(layout->hbFont, HB_OT_METRICS_TAG_CAP_HEIGHT, &layout->hbMaxHeight);
}

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

   /**************************
    * glyph cache allocation *
    *************************/
   layout->glyphCache = calloc(U16_MAX, sizeof(struct GlyphInfo));

   _layoutInitHarfbuzz(layout);
   _layoutLoadFontMetrics(layout);
}

void layoutDeInit(struct Layout *layout)
{
   hb_face_destroy(layout->hbFace);
   hb_font_destroy(layout->hbFont);
   hb_gpu_draw_destroy(layout->hbDraw);

   free(layout->glyphQuadVertices);
   free(layout->fontPath);
}
