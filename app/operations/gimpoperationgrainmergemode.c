/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpoperationgrainmergemode.c
 * Copyright (C) 2008 Michael Natterer <mitch@gimp.org>
 *               2012 Ville Sokk <ville.sokk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gegl-plugin.h>

#include "operations-types.h"

#include "gimpoperationgrainmergemode.h"


static void     gimp_operation_grain_merge_mode_prepare (GeglOperation       *operation);
static gboolean gimp_operation_grain_merge_mode_process (GeglOperation       *operation,
                                                         void                *in_buf,
                                                         void                *aux_buf,
                                                         void                *out_buf,
                                                         glong                samples,
                                                         const GeglRectangle *roi,
                                                         gint                 level);


G_DEFINE_TYPE (GimpOperationGrainMergeMode, gimp_operation_grain_merge_mode,
               GIMP_TYPE_OPERATION_POINT_LAYER_MODE)


static void
gimp_operation_grain_merge_mode_class_init (GimpOperationGrainMergeModeClass *klass)
{
  GeglOperationClass              *operation_class;
  GeglOperationPointComposerClass *point_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  point_class     = GEGL_OPERATION_POINT_COMPOSER_CLASS (klass);

  gegl_operation_class_set_keys (operation_class,
                                 "name",        "gimp:grain-merge-mode",
                                 "description", "GIMP grain merge mode operation",
                                 NULL);

  operation_class->prepare = gimp_operation_grain_merge_mode_prepare;
  point_class->process     = gimp_operation_grain_merge_mode_process;
}

static void
gimp_operation_grain_merge_mode_init (GimpOperationGrainMergeMode *self)
{
}

static void
gimp_operation_grain_merge_mode_prepare (GeglOperation *operation)
{
  const Babl *format = babl_format ("R'G'B'A float");

  gegl_operation_set_format (operation, "input",  format);
  gegl_operation_set_format (operation, "aux",    format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
gimp_operation_grain_merge_mode_process (GeglOperation       *operation,
                                         void                *in_buf,
                                         void                *aux_buf,
                                         void                *out_buf,
                                         glong                samples,
                                         const GeglRectangle *roi,
                                         gint                 level)
{
  gfloat *in    = in_buf;
  gfloat *layer = aux_buf;
  gfloat *out   = out_buf;

  while (samples--)
    {
      gint b;
      gfloat comp_alpha = MIN (in[ALPHA], layer[ALPHA]);
      gfloat new_alpha  = in[ALPHA] + (1 - in[ALPHA]) * comp_alpha;
      gfloat ratio      = comp_alpha / new_alpha;

      for (b = RED; b < ALPHA; b++)
        {
          gfloat comp = in[b] + layer[b] - 0.5;
          comp = CLAMP (comp, 0, 1);

          out[b] = comp * ratio + in[b] * (1 - ratio) + 0.0001;
        }

      out[ALPHA] = in[ALPHA];

      in    += 4;
      layer += 4;
      out   += 4;
    }

  return TRUE;
}
