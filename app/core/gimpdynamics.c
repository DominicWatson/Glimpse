/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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

#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"

#include "core-types.h"

#include "gimpcurve.h"
#include "gimpdynamics.h"
#include "gimpdynamics-load.h"
#include "gimpdynamics-save.h"
#include "gimpdynamicsoutput.h"

#include "gimp-intl.h"


#define DEFAULT_NAME "Nameless dynamics"

enum
{
  PROP_0,

  PROP_NAME,

  PROP_OPACITY_OUTPUT,
  PROP_HARDNESS_OUTPUT,
  PROP_RATE_OUTPUT,
  PROP_SIZE_OUTPUT,
  PROP_ASPECT_RATIO_OUTPUT,
  PROP_COLOR_OUTPUT,
  PROP_ANGLE_OUTPUT,
  PROP_JITTER_OUTPUT
};


static void          gimp_dynamics_finalize      (GObject      *object);
static void          gimp_dynamics_set_property  (GObject      *object,
                                                  guint         property_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);
static void          gimp_dynamics_get_property  (GObject      *object,
                                                  guint         property_id,
                                                  GValue       *value,
                                                  GParamSpec   *pspec);

static const gchar * gimp_dynamics_get_extension (GimpData     *data);

static GimpDynamicsOutput *
                     gimp_dynamics_create_output (GimpDynamics     *dynamics,
                                                  const gchar      *name);
static void          gimp_dynamics_output_notify (GObject          *output,
                                                  const GParamSpec *pspec,
                                                  GimpDynamics     *dynamics);


G_DEFINE_TYPE (GimpDynamics, gimp_dynamics,
               GIMP_TYPE_DATA)

#define parent_class gimp_dynamics_parent_class


static void
gimp_dynamics_class_init (GimpDynamicsClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GimpDataClass *data_class   = GIMP_DATA_CLASS (klass);

  object_class->finalize     = gimp_dynamics_finalize;
  object_class->set_property = gimp_dynamics_set_property;
  object_class->get_property = gimp_dynamics_get_property;

  data_class->save           = gimp_dynamics_save;
  data_class->get_extension  = gimp_dynamics_get_extension;

  GIMP_CONFIG_INSTALL_PROP_STRING  (object_class, PROP_NAME,
                                    "name", NULL,
                                    DEFAULT_NAME,
                                    GIMP_PARAM_STATIC_STRINGS);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_OPACITY_OUTPUT,
                                   "opacity-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_HARDNESS_OUTPUT,
                                   "hardness-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_RATE_OUTPUT,
                                   "rate-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_SIZE_OUTPUT,
                                   "size-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_ASPECT_RATIO_OUTPUT,
                                   "aspect-ratio-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_COLOR_OUTPUT,
                                   "color-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_ANGLE_OUTPUT,
                                   "angle-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_JITTER_OUTPUT,
                                   "jitter-output", NULL,
                                   GIMP_TYPE_DYNAMICS_OUTPUT,
                                   GIMP_CONFIG_PARAM_AGGREGATE);
}

static void
gimp_dynamics_init (GimpDynamics *dynamics)
{
  dynamics->opacity_dynamics      = gimp_dynamics_create_output (dynamics,
                                                                 "opacity-output");
  dynamics->hardness_dynamics     = gimp_dynamics_create_output (dynamics,
                                                                 "hardness-output");
  dynamics->rate_dynamics         = gimp_dynamics_create_output (dynamics,
                                                                 "rate-output");
  dynamics->size_dynamics         = gimp_dynamics_create_output (dynamics,
                                                                 "size-output");
  dynamics->aspect_ratio_dynamics = gimp_dynamics_create_output (dynamics,
                                                                 "aspect-ratio-output");
  dynamics->color_dynamics        = gimp_dynamics_create_output (dynamics,
                                                                 "color-output");
  dynamics->angle_dynamics        = gimp_dynamics_create_output (dynamics,
                                                                 "angle-output");
  dynamics->jitter_dynamics       = gimp_dynamics_create_output (dynamics,
                                                                 "jitter-output");
}

static void
gimp_dynamics_finalize (GObject *object)
{
  GimpDynamics *dynamics = GIMP_DYNAMICS (object);

  g_object_unref (dynamics->opacity_dynamics);
  g_object_unref (dynamics->hardness_dynamics);
  g_object_unref (dynamics->rate_dynamics);
  g_object_unref (dynamics->size_dynamics);
  g_object_unref (dynamics->aspect_ratio_dynamics);
  g_object_unref (dynamics->color_dynamics);
  g_object_unref (dynamics->angle_dynamics);
  g_object_unref (dynamics->jitter_dynamics);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_dynamics_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GimpDynamics       *dynamics    = GIMP_DYNAMICS (object);
  GimpDynamicsOutput *src_output  = NULL;
  GimpDynamicsOutput *dest_output = NULL;

  switch (property_id)
    {
    case PROP_NAME:
      gimp_object_set_name (GIMP_OBJECT (dynamics), g_value_get_string (value));
      break;

    case PROP_OPACITY_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->opacity_dynamics;
      break;

    case PROP_HARDNESS_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->hardness_dynamics;
      break;

    case PROP_RATE_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->rate_dynamics;
      break;

    case PROP_SIZE_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->size_dynamics;
      break;

    case PROP_ASPECT_RATIO_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->aspect_ratio_dynamics;
      break;

    case PROP_COLOR_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->color_dynamics;
      break;

    case PROP_ANGLE_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->angle_dynamics;
      break;

    case PROP_JITTER_OUTPUT:
      src_output  = g_value_get_object (value);
      dest_output = dynamics->jitter_dynamics;
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }

  if (src_output && dest_output)
    {
      gimp_config_copy (GIMP_CONFIG (src_output),
                        GIMP_CONFIG (dest_output),
                        GIMP_CONFIG_PARAM_SERIALIZE);
    }
}

static void
gimp_dynamics_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GimpDynamics *dynamics = GIMP_DYNAMICS (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gimp_object_get_name (dynamics));
      break;

    case PROP_OPACITY_OUTPUT:
      g_value_set_object (value, dynamics->opacity_dynamics);
      break;

    case PROP_HARDNESS_OUTPUT:
      g_value_set_object (value, dynamics->hardness_dynamics);
      break;

    case PROP_RATE_OUTPUT:
      g_value_set_object (value, dynamics->rate_dynamics);
      break;

    case PROP_SIZE_OUTPUT:
      g_value_set_object (value, dynamics->size_dynamics);
      break;

    case PROP_ASPECT_RATIO_OUTPUT:
      g_value_set_object (value, dynamics->aspect_ratio_dynamics);
      break;

    case PROP_COLOR_OUTPUT:
      g_value_set_object (value, dynamics->color_dynamics);
      break;

    case PROP_ANGLE_OUTPUT:
      g_value_set_object (value, dynamics->angle_dynamics);
      break;

    case PROP_JITTER_OUTPUT:
      g_value_set_object (value, dynamics->jitter_dynamics);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static const gchar *
gimp_dynamics_get_extension (GimpData *data)
{
  return GIMP_DYNAMICS_FILE_EXTENSION;
}


/*  public functions  */

GimpData *
gimp_dynamics_new (const gchar *name)
{
  return g_object_new (GIMP_TYPE_DYNAMICS,
                       "name", name,
                       NULL);
}

GimpData *
gimp_dynamics_get_standard (void)
{
  static GimpData *standard_dynamics = NULL;

  if (! standard_dynamics)
    {
      standard_dynamics = gimp_dynamics_new ("Standard dynamics");

      standard_dynamics->dirty = FALSE;
      gimp_data_make_internal (standard_dynamics,
                               "gimp-dynamics-standard");

      g_object_ref (standard_dynamics);
    }

  return standard_dynamics;
}

gboolean
gimp_dynamics_input_fade_enabled (GimpDynamics *dynamics)
{
  return (dynamics->opacity_dynamics->fade      ||
          dynamics->hardness_dynamics->fade     ||
          dynamics->rate_dynamics->fade         ||
          dynamics->size_dynamics->fade         ||
          dynamics->aspect_ratio_dynamics->fade ||
          dynamics->color_dynamics->fade        ||
          dynamics->jitter_dynamics->fade       ||
          dynamics->angle_dynamics->fade);
}


/*  private functions  */

static GimpDynamicsOutput *
gimp_dynamics_create_output (GimpDynamics *dynamics,
                             const gchar  *name)
{
  GimpDynamicsOutput *output = gimp_dynamics_output_new (name);

  g_signal_connect (output, "notify",
                    G_CALLBACK (gimp_dynamics_output_notify),
                    dynamics);

  return output;
}

static void
gimp_dynamics_output_notify (GObject          *output,
                             const GParamSpec *pspec,
                             GimpDynamics     *dynamics)
{
  g_object_notify (G_OBJECT (dynamics), gimp_object_get_name (output));
}
