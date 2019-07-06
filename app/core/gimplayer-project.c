/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimplayer-project.h
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>

#include "core-types.h"

#include "base/pixel-region.h"
#include "base/tile-manager.h"

#include "paint-funcs/paint-funcs.h"

#include "gegl/gimp-babl-compat.h"

#include "gimpimage.h"
#include "gimplayer.h"
#include "gimplayer-project.h"
#include "gimplayermask.h"


void
gimp_layer_project_region (GimpDrawable *drawable,
                           gint          x,
                           gint          y,
                           gint          width,
                           gint          height,
                           PixelRegion  *projPR,
                           gboolean      combine)
{
  GimpLayer     *layer = GIMP_LAYER (drawable);
  GimpLayerMask *mask  = gimp_layer_get_mask (layer);

  if (mask && gimp_layer_get_show_mask (layer))
    {
      /*  If we're showing the layer mask instead of the layer...  */

      PixelRegion  srcPR;
      GeglBuffer  *temp_buffer;

      gimp_drawable_init_src_region (GIMP_DRAWABLE (mask), &srcPR,
                                     x, y, width, height,
                                     &temp_buffer);

      copy_gray_to_region (&srcPR, projPR);

      if (temp_buffer)
        g_object_unref (temp_buffer);
    }
  else
    {
      /*  Otherwise, normal  */

      GimpImage       *image = gimp_item_get_image (GIMP_ITEM (layer));
      PixelRegion      srcPR;
      PixelRegion      maskPR;
      PixelRegion     *mask_pr           = NULL;
      const guchar    *colormap          = NULL;
      GeglBuffer      *temp_mask_buffer  = NULL;
      GeglBuffer      *temp_layer_buffer = NULL;
      InitialMode      initial_mode;
      CombinationMode  combination_mode;
      gboolean         visible[MAX_CHANNELS];

      gimp_drawable_init_src_region (drawable, &srcPR,
                                     x, y, width, height,
                                     &temp_layer_buffer);

      if (mask && gimp_layer_get_apply_mask (layer))
        {
          gimp_drawable_init_src_region (GIMP_DRAWABLE (mask), &maskPR,
                                         x, y, width, height,
                                         &temp_mask_buffer);
          mask_pr = &maskPR;
        }

      /*  Based on the type of the layer, project the layer onto the
       *  projection image...
       */
      switch (gimp_babl_format_get_image_type (gimp_drawable_get_format (drawable)))
        {
        case GIMP_RGB_IMAGE:
        case GIMP_GRAY_IMAGE:
          initial_mode     = INITIAL_INTENSITY;
          combination_mode = COMBINE_INTEN_A_INTEN;
          break;

        case GIMP_RGBA_IMAGE:
        case GIMP_GRAYA_IMAGE:
          initial_mode     = INITIAL_INTENSITY_ALPHA;
          combination_mode = COMBINE_INTEN_A_INTEN_A;
          break;

        case GIMP_INDEXED_IMAGE:
          colormap         = gimp_drawable_get_colormap (drawable),
          initial_mode     = INITIAL_INDEXED;
          combination_mode = COMBINE_INTEN_A_INDEXED;
          break;

        case GIMP_INDEXEDA_IMAGE:
          colormap         = gimp_drawable_get_colormap (drawable),
          initial_mode     = INITIAL_INDEXED_ALPHA;
          combination_mode = COMBINE_INTEN_A_INDEXED_A;
         break;

        default:
          g_assert_not_reached ();
          break;
        }

      gimp_image_get_visible_array (image, visible);

      if (combine)
        {
          combine_regions (projPR, &srcPR, projPR, mask_pr,
                           colormap,
                           gimp_layer_get_opacity (layer) * 255.999,
                           gimp_layer_get_mode (layer),
                           visible,
                           combination_mode);
        }
      else
        {
          initial_region (&srcPR, projPR, mask_pr,
                          colormap,
                          gimp_layer_get_opacity (layer) * 255.999,
                          gimp_layer_get_mode (layer),
                          visible,
                          initial_mode);
        }

      if (temp_layer_buffer)
        g_object_unref (temp_layer_buffer);

      if (temp_mask_buffer)
        g_object_unref (temp_mask_buffer);
    }
}
