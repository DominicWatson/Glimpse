/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include "gegl/gimp-gegl-utils.h"

#include "gimplayer.h"
#include "gimppickable.h"
#include "gimpprojectable.h"
#include "gimpprojection.h"
#include "gimpprojection-construct.h"


/*  local function prototypes  */

static void   gimp_projection_construct_gegl   (GimpProjection *proj,
                                                gint            x,
                                                gint            y,
                                                gint            w,
                                                gint            h);
static void   gimp_projection_construct_legacy (GimpProjection *proj,
                                                gint            x,
                                                gint            y,
                                                gint            w,
                                                gint            h);
static void   gimp_projection_initialize       (GimpProjection *proj,
                                                gint            x,
                                                gint            y,
                                                gint            w,
                                                gint            h);


/*  public functions  */

void
gimp_projection_construct (GimpProjection *proj,
                           gint            x,
                           gint            y,
                           gint            w,
                           gint            h)
{
  g_return_if_fail (GIMP_IS_PROJECTION (proj));

  /*  First, determine if the projection image needs to be
   *  initialized--this is the case when there are no visible
   *  layers that cover the entire canvas--either because layers
   *  are offset or only a floating selection is visible
   */
  gimp_projection_initialize (proj, x, y, w, h);

  /*  call functions which process the list of layers and
   *  the list of channels
   */
  if (proj->use_gegl)
    {
      gimp_projection_construct_gegl (proj, x, y, w, h);
    }
  else
    {
      proj->construct_flag = FALSE;

      gimp_projection_construct_legacy (proj, x, y, w, h);
    }
}


/*  private functions  */

static void
gimp_projection_construct_gegl (GimpProjection *proj,
                                gint            x,
                                gint            y,
                                gint            w,
                                gint            h)
{
  GeglRectangle  rect = { x, y, w, h };

  if (! proj->processor)
    {
      GeglNode *sink = gimp_projection_get_sink_node (proj);

      proj->processor = gegl_node_new_processor (sink, &rect);
    }
  else
    {
      gegl_processor_set_rectangle (proj->processor, &rect);
    }

  while (gegl_processor_work (proj->processor, NULL));
}

static void
gimp_projection_construct_legacy (GimpProjection *proj,
                                  gint            x,
                                  gint            y,
                                  gint            w,
                                  gint            h)
{
  GList *list;
  GList *reverse_list = NULL;
  gint   proj_off_x;
  gint   proj_off_y;

  for (list = gimp_projectable_get_channels (proj->projectable);
       list;
       list = g_list_next (list))
    {
      if (gimp_item_get_visible (GIMP_ITEM (list->data)))
        {
          reverse_list = g_list_prepend (reverse_list, list->data);
        }
    }

  for (list = gimp_projectable_get_layers (proj->projectable);
       list;
       list = g_list_next (list))
    {
      GimpLayer *layer = list->data;

      if (! gimp_layer_is_floating_sel (layer) &&
          gimp_item_get_visible (GIMP_ITEM (layer)))
        {
          /*  only add layers that are visible and not floating selections
           *  to the list
           */
          reverse_list = g_list_prepend (reverse_list, layer);
        }
    }

  gimp_projectable_get_offset (proj->projectable, &proj_off_x, &proj_off_y);

  for (list = reverse_list; list; list = g_list_next (list))
    {
      GimpItem    *item = list->data;
      GeglBuffer  *proj_buffer;
      PixelRegion  projPR;
      gint         x1, y1;
      gint         x2, y2;
      gint         off_x;
      gint         off_y;

      gimp_item_get_offset (item, &off_x, &off_y);

      /*  subtract the projectable's offsets because the list of
       *  update areas is in tile-pyramid coordinates, but our
       *  external API is always in terms of image coordinates.
       */
      off_x -= proj_off_x;
      off_y -= proj_off_y;

      x1 = CLAMP (off_x,                               x, x + w);
      y1 = CLAMP (off_y,                               y, y + h);
      x2 = CLAMP (off_x + gimp_item_get_width  (item), x, x + w);
      y2 = CLAMP (off_y + gimp_item_get_height (item), y, y + h);

      proj_buffer = gimp_pickable_get_buffer (GIMP_PICKABLE (proj));

      pixel_region_init (&projPR,
                         gimp_gegl_buffer_get_tiles (proj_buffer),
                         x1, y1, x2 - x1, y2 - y1,
                         TRUE);

      gimp_drawable_project_region (GIMP_DRAWABLE (item),
                                    x1 - off_x, y1 - off_y,
                                    x2 - x1,    y2 - y1,
                                    &projPR,
                                    proj->construct_flag);

      proj->construct_flag = TRUE;  /*  something was projected  */
    }

  g_list_free (reverse_list);
}

/**
 * gimp_projection_initialize:
 * @proj: A #GimpProjection.
 * @x:
 * @y:
 * @w:
 * @h:
 *
 * This function determines whether a visible layer with combine mode
 * Normal provides complete coverage over the specified area.  If not,
 * the projection is initialized to transparent black.
 */
static void
gimp_projection_initialize (GimpProjection *proj,
                            gint            x,
                            gint            y,
                            gint            w,
                            gint            h)
{
  GList    *list;
  gint      proj_off_x;
  gint      proj_off_y;
  gboolean  coverage = FALSE;

  gimp_projectable_get_offset (proj->projectable, &proj_off_x, &proj_off_y);

  for (list = gimp_projectable_get_layers (proj->projectable);
       list;
       list = g_list_next (list))
    {
      GimpLayer    *layer    = list->data;
      GimpDrawable *drawable = GIMP_DRAWABLE (layer);
      GimpItem     *item     = GIMP_ITEM (layer);
      gint          off_x, off_y;

      gimp_item_get_offset (item, &off_x, &off_y);

      /*  subtract the projectable's offsets because the list of
       *  update areas is in tile-pyramid coordinates, but our
       *  external API is always in terms of image coordinates.
       */
      off_x -= proj_off_x;
      off_y -= proj_off_y;

      if (gimp_item_get_visible (item)                          &&
          ! gimp_drawable_has_alpha (drawable)                  &&
          ! gimp_layer_get_mask (layer)                         &&
          gimp_layer_get_mode (layer) == GIMP_NORMAL_MODE       &&
          gimp_layer_get_opacity (layer) == GIMP_OPACITY_OPAQUE &&
          off_x <= x                                            &&
          off_y <= y                                            &&
          (off_x + gimp_item_get_width  (item)) >= (x + w)      &&
          (off_y + gimp_item_get_height (item)) >= (y + h))
        {
          coverage = TRUE;
          break;
        }
    }

  if (! coverage)
    {
      GeglBuffer    *buffer;

      if (proj->use_gegl)
        {
          /* GEGL should really do this for us... */
          gegl_node_get (gimp_projection_get_sink_node (proj),
                         "buffer", &buffer, NULL);
        }
      else
        {
          GeglBuffer  *buffer = gimp_pickable_get_buffer (GIMP_PICKABLE (proj));
          TileManager *tiles  = gimp_gegl_buffer_get_tiles (buffer);
          const Babl  *format = gimp_pickable_get_format (GIMP_PICKABLE (proj));

          buffer = gimp_tile_manager_create_buffer (tiles, format);
        }

      gegl_buffer_clear (buffer, GEGL_RECTANGLE (x, y, w, h));
      g_object_unref (buffer);
    }
}
