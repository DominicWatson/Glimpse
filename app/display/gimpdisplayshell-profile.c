/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
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

#include <lcms2.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "config/gimpcoreconfig.h"

#include "gegl/gimp-babl.h"

#include "core/gimpimage.h"
#include "core/gimppickable.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-filter.h"
#include "gimpdisplayshell-profile.h"
#include "gimpdisplayxfer.h"

#include "gimp-intl.h"


void
gimp_display_shell_profile_dispose (GimpDisplayShell *shell)
{
  if (shell->profile_transform)
    {
      cmsDeleteTransform (shell->profile_transform);
      shell->profile_transform   = NULL;
      shell->profile_src_format  = NULL;
      shell->profile_dest_format = NULL;
    }

  if (shell->profile_buffer)
    {
      g_object_unref (shell->profile_buffer);
      shell->profile_buffer = NULL;
      shell->profile_data   = NULL;
      shell->profile_stride = 0;
    }
}

void
gimp_display_shell_profile_update (GimpDisplayShell *shell)
{
  GimpImage        *image;
  GimpColorConfig  *config;
  GimpColorProfile *src_profile;
  const Babl       *src_format;
  const Babl       *dest_format;

  gimp_display_shell_profile_dispose (shell);

  image = gimp_display_get_image (shell->display);

  g_printerr ("gimp_display_shell_profile_update\n");

  if (! image)
    return;

  config = GIMP_CORE_CONFIG (shell->display->config)->color_management;

  src_profile = gimp_color_managed_get_color_profile (GIMP_COLOR_MANAGED (shell));

  src_format = gimp_babl_format (GIMP_RGB,
                                 gimp_image_get_precision (image),
                                 TRUE);

  if (gimp_display_shell_has_filter (shell) ||
      ! gimp_display_shell_profile_can_convert_to_u8 (shell))
    {
      dest_format = shell->filter_format;
    }
  else
    {
      dest_format = babl_format ("R'G'B'A u8");
    }

  g_printerr ("src_format: %s\n", babl_get_name (src_format));
  g_printerr ("dest_format: %s\n", babl_get_name (dest_format));

  shell->profile_transform =
    gimp_widget_get_color_transform (gtk_widget_get_toplevel (GTK_WIDGET (shell)),
                                     config,
                                     src_profile,
                                     &src_format,
                                     &dest_format);

  if (shell->profile_transform)
    {
      gint w = GIMP_DISPLAY_RENDER_BUF_WIDTH  * GIMP_DISPLAY_RENDER_MAX_SCALE;
      gint h = GIMP_DISPLAY_RENDER_BUF_HEIGHT * GIMP_DISPLAY_RENDER_MAX_SCALE;

      shell->profile_src_format  = src_format;
      shell->profile_dest_format = dest_format;

      shell->profile_data =
        gegl_malloc (w * h * babl_format_get_bytes_per_pixel (src_format));

      shell->profile_stride =
        w * babl_format_get_bytes_per_pixel (src_format);

      shell->profile_buffer =
        gegl_buffer_linear_new_from_data (shell->profile_data,
                                          src_format,
                                          GEGL_RECTANGLE (0, 0, w, h),
                                          GEGL_AUTO_ROWSTRIDE,
                                          (GDestroyNotify) gegl_free,
                                          shell->profile_data);
    }
}

gboolean
gimp_display_shell_profile_can_convert_to_u8 (GimpDisplayShell *shell)
{
  GimpImage *image;

  image = gimp_display_get_image (shell->display);

  if (image)
    {
      switch (gimp_image_get_component_type (image))
        {
        case GIMP_COMPONENT_TYPE_U8:
#if 0
          /* would like to convert directly for these too, but it
           * produces inferior results, see bug 750874
           */
        case GIMP_COMPONENT_TYPE_U16:
        case GIMP_COMPONENT_TYPE_U32:
#endif
          return TRUE;

        default:
          break;
        }
    }

  return FALSE;
}

void
gimp_display_shell_profile_convert_buffer (GimpDisplayShell *shell,
                                           GeglBuffer       *src_buffer,
                                           GeglRectangle    *src_area,
                                           GeglBuffer       *dest_buffer,
                                           GeglRectangle    *dest_area)
{
  GeglBufferIterator *iter;
  const Babl         *fish;

  if (! shell->profile_transform)
    return;

  iter = gegl_buffer_iterator_new (src_buffer, src_area, 0,
                                   shell->profile_src_format,
                                   GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

  gegl_buffer_iterator_add (iter, dest_buffer, dest_area, 0,
                            shell->profile_dest_format,
                            GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE);

  fish = babl_fish (shell->profile_src_format,
                    shell->profile_dest_format);

  while (gegl_buffer_iterator_next (iter))
    {
      gpointer src_data  = iter->data[0];
      gpointer dest_data = iter->data[1];

      babl_process (fish, src_data, dest_data, iter->length);

      cmsDoTransform (shell->profile_transform,
                      src_data, dest_data,
                      iter->length);
    }
}
