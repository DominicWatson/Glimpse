/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2009 Martin Nordholts
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

#include <string.h>

#include <gegl.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "core/core-types.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "core/gimpdrawable.h"
#include "core/gimpgrid.h"
#include "core/gimpguide.h"
#include "core/gimpimage.h"
#include "core/gimpimage-grid.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-sample-points.h"
#include "core/gimplayer.h"
#include "core/gimpsamplepoint.h"
#include "core/gimpselection.h"

#include "vectors/gimpanchor.h"
#include "vectors/gimpbezierstroke.h"
#include "vectors/gimpvectors.h"

#include "file/file-open.h"
#include "file/file-procedure.h"
#include "file/file-save.h"

#include "plug-in/gimppluginmanager.h"

#include "tests.h"

#include "gimp-app-test-utils.h"


#define GIMP_MAINIMAGE_WIDTH            100
#define GIMP_MAINIMAGE_HEIGHT           90
#define GIMP_MAINIMAGE_TYPE             GIMP_RGB

#define GIMP_MAINIMAGE_LAYER1_NAME      "layer1"
#define GIMP_MAINIMAGE_LAYER1_WIDTH     50
#define GIMP_MAINIMAGE_LAYER1_HEIGHT    51
#define GIMP_MAINIMAGE_LAYER1_TYPE      GIMP_RGBA_IMAGE
#define GIMP_MAINIMAGE_LAYER1_OPACITY   1.0
#define GIMP_MAINIMAGE_LAYER1_MODE      GIMP_NORMAL_MODE

#define GIMP_MAINIMAGE_LAYER2_NAME      "layer2"
#define GIMP_MAINIMAGE_LAYER2_WIDTH     25
#define GIMP_MAINIMAGE_LAYER2_HEIGHT    251
#define GIMP_MAINIMAGE_LAYER2_TYPE      GIMP_RGB_IMAGE
#define GIMP_MAINIMAGE_LAYER2_OPACITY   0.0
#define GIMP_MAINIMAGE_LAYER2_MODE      GIMP_MULTIPLY_MODE

#define GIMP_MAINIMAGE_VGUIDE1_POS      42
#define GIMP_MAINIMAGE_VGUIDE2_POS      82
#define GIMP_MAINIMAGE_HGUIDE1_POS      3
#define GIMP_MAINIMAGE_HGUIDE2_POS      4

#define GIMP_MAINIMAGE_SAMPLEPOINT1_X   10
#define GIMP_MAINIMAGE_SAMPLEPOINT1_Y   12
#define GIMP_MAINIMAGE_SAMPLEPOINT2_X   41
#define GIMP_MAINIMAGE_SAMPLEPOINT2_Y   49

#define GIMP_MAINIMAGE_RESOLUTIONX      400
#define GIMP_MAINIMAGE_RESOLUTIONY      410

#define GIMP_MAINIMAGE_PARASITE_NAME    "test-parasite"
#define GIMP_MAINIMAGE_PARASITE_DATA    "foo"
#define GIMP_MAINIMAGE_PARASITE_SIZE    4                /* 'f' 'o' 'o' '\0' */

#define GIMP_MAINIMAGE_COMMENT          "Created with code from "\
                                        "app/tests/test-xcf.c in the GIMP "\
                                        "source tree, i.e. it was not created "\
                                        "manually and may thus look weird if "\
                                        "opened and inspected in GIMP."

#define GIMP_MAINIMAGE_UNIT             GIMP_UNIT_PICA

#define GIMP_MAINIMAGE_GRIDXSPACING     25.0
#define GIMP_MAINIMAGE_GRIDYSPACING     27.0

#define GIMP_MAINIMAGE_CHANNEL1_NAME    "channel1"
#define GIMP_MAINIMAGE_CHANNEL1_WIDTH   GIMP_MAINIMAGE_WIDTH
#define GIMP_MAINIMAGE_CHANNEL1_HEIGHT  GIMP_MAINIMAGE_HEIGHT
#define GIMP_MAINIMAGE_CHANNEL1_COLOR   { 1.0, 0.0, 1.0, 1.0 }

#define GIMP_MAINIMAGE_SELECTION_X      5
#define GIMP_MAINIMAGE_SELECTION_Y      6
#define GIMP_MAINIMAGE_SELECTION_W      7
#define GIMP_MAINIMAGE_SELECTION_H      8

#define GIMP_MAINIMAGE_VECTORS1_NAME    "vectors1"
#define GIMP_MAINIMAGE_VECTORS1_COORDS  { { 11.0, 12.0, /* pad zeroes */ },\
                                          { 21.0, 22.0, /* pad zeroes */ },\
                                          { 31.0, 32.0, /* pad zeroes */ }, }

#define GIMP_MAINIMAGE_VECTORS2_NAME    "vectors2"
#define GIMP_MAINIMAGE_VECTORS2_COORDS  { { 911.0, 912.0, /* pad zeroes */ },\
                                          { 921.0, 922.0, /* pad zeroes */ },\
                                          { 931.0, 932.0, /* pad zeroes */ }, }


typedef struct
{
  gint avoid_sizeof_zero;
} GimpTestFixture;


static void        gimp_write_and_read_gimp_2_6_format         (GimpTestFixture *fixture,
                                                                gconstpointer    data);
static void        gimp_write_and_read_gimp_2_6_format_unusual (GimpTestFixture *fixture,
                                                                gconstpointer    data);
static void        gimp_load_gimp_2_6_file                     (GimpTestFixture *fixture,
                                                                gconstpointer    data);
GimpImage        * gimp_test_load_image                        (Gimp            *gimp,
                                                                const gchar     *uri);
static void        gimp_write_and_read_file                    (gboolean         with_unusual_stuff,
                                                                gboolean         compat_paths);
static GimpImage * gimp_create_mainimage                       (gboolean         with_unusual_stuff,
                                                                gboolean         compat_paths);
static void        gimp_assert_mainimage                       (GimpImage       *image,
                                                                gboolean         with_unusual_stuff,
                                                                gboolean         compat_paths);


static Gimp *gimp = NULL;


/**
 * gimp_write_and_read_gimp_2_6_format:
 * @fixture:
 * @data:
 *
 * Do a write and read test on a file that could as well be
 * constructed with GIMP 2.6.
 **/
static void
gimp_write_and_read_gimp_2_6_format (GimpTestFixture *fixture,
                                     gconstpointer    data)
{
  gimp_write_and_read_file (FALSE /*with_unusual_stuff*/,
                            FALSE /*compat_paths*/);
}

/**
 * gimp_write_and_read_gimp_2_6_format_unusual:
 * @fixture:
 * @data:
 *
 * Do a write and read test on a file that could as well be
 * constructed with GIMP 2.6, and make it unusual, like compatible
 * vectors and with a floating selection.
 **/
static void
gimp_write_and_read_gimp_2_6_format_unusual (GimpTestFixture *fixture,
                                             gconstpointer    data)
{
  gimp_write_and_read_file (TRUE /*with_unusual_stuff*/,
                            TRUE /*compat_paths*/);
}

/**
 * gimp_load_gimp_2_6_file:
 * @fixture:
 * @data:
 *
 * Loads a file created with GIMP 2.6 and makes sure it loaded as
 * expected.
 **/
static void
gimp_load_gimp_2_6_file (GimpTestFixture *fixture,
                         gconstpointer    data)
{
  GimpImage *image = NULL;
  gchar     *uri   = NULL;

  uri = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"),
                          "app/tests/files/gimp-2-6-file.xcf",
                          NULL);

  image = gimp_test_load_image (gimp, uri);

  /* The image file was constructed by running
   * gimp_write_and_read_file (FALSE, FALSE) in GIMP 2.6 by
   * copy-pasting the code to GIMP 2.6 and adapting it to changes in
   * the core API, so we can use gimp_assert_mainimage() to make sure
   * the file was loaded successfully.
   */
  gimp_assert_mainimage (image,
                         FALSE /*with_unusual_stuff*/,
                         FALSE /*compat_paths*/);
}

GimpImage *
gimp_test_load_image (Gimp        *gimp,
                      const gchar *uri)
{
  GimpPlugInProcedure *proc     = NULL;
  GimpImage           *image    = NULL;
  GimpPDBStatusType    not_used = 0;

  proc = file_procedure_find (gimp->plug_in_manager->load_procs,
                              uri,
                              NULL /*error*/);
  image = file_open_image (gimp,
                           gimp_get_user_context (gimp),
                           NULL /*progress*/,
                           uri,
                           "irrelevant" /*entered_filename*/,
                           FALSE /*as_new*/,
                           proc,
                           GIMP_RUN_NONINTERACTIVE,
                           &not_used /*status*/,
                           NULL /*mime_type*/,
                           NULL /*error*/);

  return image;
}

/**
 * gimp_write_and_read_file:
 *
 * Constructs the main test image and asserts its state, writes it to
 * a file, reads the image from the file, and asserts the state of the
 * loaded file. The function takes various parameters so the same
 * function can be used for different formats.
 **/
static void
gimp_write_and_read_file (gboolean with_unusual_stuff,
                          gboolean compat_paths)
{
  GimpImage           *image        = NULL;
  GimpImage           *loaded_image = NULL;
  GimpPlugInProcedure *proc         = NULL;
  gchar               *uri          = NULL;

  /* Create the image */
  image = gimp_create_mainimage (with_unusual_stuff,
                                 compat_paths);

  /* Assert valid state */
  gimp_assert_mainimage (image,
                         with_unusual_stuff,
                         compat_paths);

  /* Write to file */
  uri  = g_build_filename (g_get_tmp_dir (), "gimp-test.xcf", NULL);
  proc = file_procedure_find (image->gimp->plug_in_manager->save_procs,
                              uri,
                              NULL /*error*/);
  file_save (gimp,
             image,
             NULL /*progress*/,
             uri,
             proc,
             GIMP_RUN_NONINTERACTIVE,
             FALSE /*change_saved_state*/,
             FALSE /*export*/,
             NULL /*error*/);

  /* Load from file */
  loaded_image = gimp_test_load_image (image->gimp, uri);
  g_free (uri);

  /* Assert on the loaded file. If success, it means that there is no
   * significant information loss when we wrote the image to a file
   * and loaded it again
   */
  gimp_assert_mainimage (loaded_image,
                         with_unusual_stuff,
                         compat_paths);
}

/**
 * gimp_create_mainimage:
 *
 * Creates the main test image, i.e. the image that we use for most of
 * our XCF testing purposes.
 *
 * Returns: The #GimpImage
 **/
static GimpImage *
gimp_create_mainimage (gboolean with_unusual_stuff,
                       gboolean compat_paths)
{
  GimpImage     *image             = NULL;
  GimpLayer     *layer             = NULL;
  GimpParasite  *parasite          = NULL;
  GimpGrid      *grid              = NULL;
  GimpChannel   *channel           = NULL;
  GimpRGB        channel_color     = GIMP_MAINIMAGE_CHANNEL1_COLOR;
  GimpChannel   *selection         = NULL;
  GimpVectors   *vectors           = NULL;
  GimpCoords     vectors1_coords[] = GIMP_MAINIMAGE_VECTORS1_COORDS;
  GimpCoords     vectors2_coords[] = GIMP_MAINIMAGE_VECTORS2_COORDS;
  GimpStroke    *stroke            = NULL;
  GimpLayerMask *layer_mask        = NULL;

  /* Image size and type */
  image = gimp_image_new (gimp,
                          GIMP_MAINIMAGE_WIDTH,
                          GIMP_MAINIMAGE_HEIGHT,
                          GIMP_MAINIMAGE_TYPE);

  /* Layers */
  layer = gimp_layer_new (image,
                          GIMP_MAINIMAGE_LAYER1_WIDTH,
                          GIMP_MAINIMAGE_LAYER1_HEIGHT,
                          GIMP_MAINIMAGE_LAYER1_TYPE,
                          GIMP_MAINIMAGE_LAYER1_NAME,
                          GIMP_MAINIMAGE_LAYER1_OPACITY,
                          GIMP_MAINIMAGE_LAYER1_MODE);
  gimp_image_add_layer (image,
                        layer,
                        NULL,
                        0,
                        FALSE/*push_undo*/);
  layer = gimp_layer_new (image,
                          GIMP_MAINIMAGE_LAYER2_WIDTH,
                          GIMP_MAINIMAGE_LAYER2_HEIGHT,
                          GIMP_MAINIMAGE_LAYER2_TYPE,
                          GIMP_MAINIMAGE_LAYER2_NAME,
                          GIMP_MAINIMAGE_LAYER2_OPACITY,
                          GIMP_MAINIMAGE_LAYER2_MODE);
  gimp_image_add_layer (image,
                        layer,
                        NULL,
                        0,
                        FALSE /*push_undo*/);

  /* Layer mask */
  layer_mask = gimp_layer_create_mask (layer,
                                       GIMP_ADD_BLACK_MASK,
                                       NULL /*channel*/);
  gimp_layer_add_mask (layer,
                       layer_mask,
                       FALSE /*push_undo*/,
                       NULL /*error*/);

  /* Image compression type
   *
   * We don't do any explicit test, only implicit when we read tile
   * data in other tests
   */

  /* Guides, note we add them in reversed order */
  gimp_image_add_hguide (image,
                         GIMP_MAINIMAGE_HGUIDE2_POS,
                         FALSE /*push_undo*/);
  gimp_image_add_hguide (image,
                         GIMP_MAINIMAGE_HGUIDE1_POS,
                         FALSE /*push_undo*/);
  gimp_image_add_vguide (image,
                         GIMP_MAINIMAGE_VGUIDE2_POS,
                         FALSE /*push_undo*/);
  gimp_image_add_vguide (image,
                         GIMP_MAINIMAGE_VGUIDE1_POS,
                         FALSE /*push_undo*/);


  /* Sample points */
  gimp_image_add_sample_point_at_pos (image,
                                      GIMP_MAINIMAGE_SAMPLEPOINT1_X,
                                      GIMP_MAINIMAGE_SAMPLEPOINT1_Y,
                                      FALSE /*push_undo*/);
  gimp_image_add_sample_point_at_pos (image,
                                      GIMP_MAINIMAGE_SAMPLEPOINT2_X,
                                      GIMP_MAINIMAGE_SAMPLEPOINT2_Y,
                                      FALSE /*push_undo*/);

  /* Tatto
   * We don't bother testing this, not yet at least
   */

  /* Resolution */
  gimp_image_set_resolution (image,
                             GIMP_MAINIMAGE_RESOLUTIONX,
                             GIMP_MAINIMAGE_RESOLUTIONY);


  /* Parasites */
  parasite = gimp_parasite_new (GIMP_MAINIMAGE_PARASITE_NAME,
                                GIMP_PARASITE_PERSISTENT,
                                GIMP_MAINIMAGE_PARASITE_SIZE,
                                GIMP_MAINIMAGE_PARASITE_DATA);
  gimp_image_parasite_attach (image,
                              parasite);
  gimp_parasite_free (parasite);
  parasite = gimp_parasite_new ("gimp-comment",
                                GIMP_PARASITE_PERSISTENT,
                                strlen (GIMP_MAINIMAGE_COMMENT) + 1,
                                GIMP_MAINIMAGE_COMMENT);
  gimp_image_parasite_attach (image, parasite);
  gimp_parasite_free (parasite);


  /* Unit */
  gimp_image_set_unit (image,
                       GIMP_MAINIMAGE_UNIT);

  /* Grid */
  grid = g_object_new (GIMP_TYPE_GRID,
                       "xspacing", GIMP_MAINIMAGE_GRIDXSPACING,
                       "yspacing", GIMP_MAINIMAGE_GRIDYSPACING,
                       NULL);
  gimp_image_set_grid (image,
                       grid,
                       FALSE /*push_undo*/);
  g_object_unref (grid);

  /* Channel */
  channel = gimp_channel_new (image,
                              GIMP_MAINIMAGE_CHANNEL1_WIDTH,
                              GIMP_MAINIMAGE_CHANNEL1_HEIGHT,
                              GIMP_MAINIMAGE_CHANNEL1_NAME,
                              &channel_color);
  gimp_image_add_channel (image,
                          channel,
                          NULL,
                          -1,
                          FALSE /*push_undo*/);

  /* Selection */
  selection = gimp_image_get_mask (image);
  gimp_channel_select_rectangle (selection,
                                 GIMP_MAINIMAGE_SELECTION_X,
                                 GIMP_MAINIMAGE_SELECTION_Y,
                                 GIMP_MAINIMAGE_SELECTION_W,
                                 GIMP_MAINIMAGE_SELECTION_H,
                                 GIMP_CHANNEL_OP_REPLACE,
                                 FALSE /*feather*/,
                                 0.0 /*feather_radius_x*/,
                                 0.0 /*feather_radius_y*/,
                                 FALSE /*push_undo*/);

  /* Vectors 1 */
  vectors = gimp_vectors_new (image,
                              GIMP_MAINIMAGE_VECTORS1_NAME);
  /* The XCF file can save vectors in two kind of ways, one old way
   * and a new way. Parameterize the way so we can test both variants,
   * i.e. gimp_vectors_compat_is_compatible() must return both TRUE
   * and FALSE.
   */
  if (! compat_paths)
    {
      gimp_item_set_visible (GIMP_ITEM (vectors),
                             TRUE,
                             FALSE /*push_undo*/);
    }
  /* TODO: Add test for non-closed stroke. The order of the anchor
   * points changes for open strokes, so it's boring to test
   */
  stroke = gimp_bezier_stroke_new_from_coords (vectors1_coords,
                                               G_N_ELEMENTS (vectors1_coords),
                                               TRUE /*closed*/);
  gimp_vectors_stroke_add (vectors, stroke);
  gimp_image_add_vectors (image,
                          vectors,
                          NULL /*parent*/,
                          -1 /*position*/,
                          FALSE /*push_undo*/);

  /* Vectors 2 */
  vectors = gimp_vectors_new (image,
                              GIMP_MAINIMAGE_VECTORS2_NAME);

  stroke = gimp_bezier_stroke_new_from_coords (vectors2_coords,
                                               G_N_ELEMENTS (vectors2_coords),
                                               TRUE /*closed*/);
  gimp_vectors_stroke_add (vectors, stroke);
  gimp_image_add_vectors (image,
                          vectors,
                          NULL /*parent*/,
                          -1 /*position*/,
                          FALSE /*push_undo*/);

  /* Some of these things are pretty unusual, parameterize the
   * inclusion of this in the written file so we can do our test both
   * with and without
   */
  if (with_unusual_stuff)
    {
      /* Floating selection */
      gimp_selection_float (GIMP_SELECTION (gimp_image_get_mask (image)),
                            gimp_image_get_active_drawable (image),
                            gimp_get_user_context (gimp),
                            TRUE /*cut_image*/,
                            0 /*off_x*/,
                            0 /*off_y*/,
                            NULL /*error*/);
    }

  /* Todo, should be tested somehow:
   *
   * - Color maps
   * - Custom user units
   * - Text layers
   * - Layer parasites
   * - Channel parasites
   * - Different tile compression methods
   */

  return image;
}

static void
gimp_assert_vectors (GimpImage   *image,
                     const gchar *name,
                     GimpCoords   coords[],
                     gsize        coords_size,
                     gboolean     visible)
{
  GimpVectors *vectors        = NULL;
  GimpStroke  *stroke         = NULL;
  GArray      *control_points = NULL;
  gboolean     closed         = FALSE;
  gint         i              = 0;

  vectors = gimp_image_get_vectors_by_name (image, name);
  stroke = gimp_vectors_stroke_get_next (vectors, NULL);
  g_assert (stroke != NULL);
  control_points = gimp_stroke_control_points_get (stroke,
                                                   &closed);
  g_assert (closed);
  g_assert_cmpint (control_points->len,
                   ==,
                   coords_size);
  for (i = 0; i < control_points->len; i++)
    {
      g_assert_cmpint (coords[i].x,
                       ==,
                       g_array_index (control_points,
                                      GimpAnchor,
                                      i).position.x);
      g_assert_cmpint (coords[i].y,
                       ==,
                       g_array_index (control_points,
                                      GimpAnchor,
                                      i).position.y);
    }

  g_assert (gimp_item_get_visible (GIMP_ITEM (vectors)) ? TRUE : FALSE ==
            visible ? TRUE : FALSE);
}

/**
 * gimp_assert_mainimage:
 * @image:
 *
 * Verifies that the passed #GimpImage contains all the information
 * that was put in it by gimp_create_mainimage().
 **/
static void
gimp_assert_mainimage (GimpImage *image,
                       gboolean   with_unusual_stuff,
                       gboolean   compat_paths)
{
  const GimpParasite *parasite               = NULL;
  GimpLayer          *layer                  = NULL;
  GList              *iter                   = NULL;
  GimpGuide          *guide                  = NULL;
  GimpSamplePoint    *sample_point           = NULL;
  gdouble             xres                   = 0.0;
  gdouble             yres                   = 0.0;
  GimpGrid           *grid                   = NULL;
  gdouble             xspacing               = 0.0;
  gdouble             yspacing               = 0.0;
  GimpChannel        *channel                = NULL;
  GimpRGB             expected_channel_color = GIMP_MAINIMAGE_CHANNEL1_COLOR;
  GimpRGB             actual_channel_color   = { 0, };
  GimpChannel        *selection              = NULL;
  gint                x1                     = -1;
  gint                y1                     = -1;
  gint                x2                     = -1;
  gint                y2                     = -1;
  gint                w                      = -1;
  gint                h                      = -1;
  GimpCoords          vectors1_coords[]      = GIMP_MAINIMAGE_VECTORS1_COORDS;
  GimpCoords          vectors2_coords[]      = GIMP_MAINIMAGE_VECTORS2_COORDS;

  /* Image size and type */
  g_assert_cmpint (gimp_image_get_width (image),
                   ==,
                   GIMP_MAINIMAGE_WIDTH);
  g_assert_cmpint (gimp_image_get_height (image),
                   ==,
                   GIMP_MAINIMAGE_HEIGHT);
  g_assert_cmpint (gimp_image_base_type (image),
                   ==,
                   GIMP_MAINIMAGE_TYPE);

  /* Layers */
  layer = gimp_image_get_layer_by_name (image,
                                        GIMP_MAINIMAGE_LAYER1_NAME);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_WIDTH);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_HEIGHT);
  g_assert_cmpint (gimp_drawable_type (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_TYPE);
  g_assert_cmpstr (gimp_object_get_name (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_NAME);
  g_assert_cmpfloat (gimp_layer_get_opacity (layer),
                     ==,
                     GIMP_MAINIMAGE_LAYER1_OPACITY);
  g_assert_cmpint (gimp_layer_get_mode (layer),
                   ==,
                   GIMP_MAINIMAGE_LAYER1_MODE);
  layer = gimp_image_get_layer_by_name (image,
                                        GIMP_MAINIMAGE_LAYER2_NAME);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_WIDTH);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_HEIGHT);
  g_assert_cmpint (gimp_drawable_type (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_TYPE);
  g_assert_cmpstr (gimp_object_get_name (GIMP_DRAWABLE (layer)),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_NAME);
  g_assert_cmpfloat (gimp_layer_get_opacity (layer),
                     ==,
                     GIMP_MAINIMAGE_LAYER2_OPACITY);
  g_assert_cmpint (gimp_layer_get_mode (layer),
                   ==,
                   GIMP_MAINIMAGE_LAYER2_MODE);

  /* Guides, note that we rely on internal ordering */
  iter = gimp_image_get_guides (image);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_VGUIDE1_POS);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_VGUIDE2_POS);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_HGUIDE1_POS);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  guide = GIMP_GUIDE (iter->data);
  g_assert_cmpint (gimp_guide_get_position (guide),
                   ==,
                   GIMP_MAINIMAGE_HGUIDE2_POS);
  iter = g_list_next (iter);
  g_assert (iter == NULL);

  /* Sample points, we rely on the same ordering as when we added
   * them, although this ordering is not a necessaity
   */
  iter = gimp_image_get_sample_points (image);
  g_assert (iter != NULL);
  sample_point = (GimpSamplePoint *) iter->data;
  g_assert_cmpint (sample_point->x,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT1_X);
  g_assert_cmpint (sample_point->y,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT1_Y);
  iter = g_list_next (iter);
  g_assert (iter != NULL);
  sample_point = (GimpSamplePoint *) iter->data;
  g_assert_cmpint (sample_point->x,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT2_X);
  g_assert_cmpint (sample_point->y,
                   ==,
                   GIMP_MAINIMAGE_SAMPLEPOINT2_Y);
  iter = g_list_next (iter);
  g_assert (iter == NULL);

  /* Resolution */
  gimp_image_get_resolution (image, &xres, &yres);
  g_assert_cmpint (xres,
                   ==,
                   GIMP_MAINIMAGE_RESOLUTIONX);
  g_assert_cmpint (yres,
                   ==,
                   GIMP_MAINIMAGE_RESOLUTIONY);

  /* Parasites */
  parasite = gimp_image_parasite_find (image,
                                       GIMP_MAINIMAGE_PARASITE_NAME);
  g_assert_cmpint (gimp_parasite_data_size (parasite),
                   ==,
                   GIMP_MAINIMAGE_PARASITE_SIZE);
  g_assert_cmpstr (gimp_parasite_data (parasite),
                   ==,
                   GIMP_MAINIMAGE_PARASITE_DATA);
  parasite = gimp_image_parasite_find (image,
                                       "gimp-comment");
  g_assert_cmpint (gimp_parasite_data_size (parasite),
                   ==,
                   strlen (GIMP_MAINIMAGE_COMMENT) + 1);
  g_assert_cmpstr (gimp_parasite_data (parasite),
                   ==,
                   GIMP_MAINIMAGE_COMMENT);

  /* Unit */
  g_assert_cmpint (gimp_image_get_unit (image),
                   ==,
                   GIMP_MAINIMAGE_UNIT);

  /* Grid */
  grid = gimp_image_get_grid (image);
  g_object_get (grid,
                "xspacing", &xspacing,
                "yspacing", &yspacing,
                NULL);
  g_assert_cmpint (xspacing,
                   ==,
                   GIMP_MAINIMAGE_GRIDXSPACING);
  g_assert_cmpint (yspacing,
                   ==,
                   GIMP_MAINIMAGE_GRIDYSPACING);


  /* Channel */
  channel = gimp_image_get_channel_by_name (image,
                                            GIMP_MAINIMAGE_CHANNEL1_NAME);
  gimp_channel_get_color (channel, &actual_channel_color);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (channel)),
                   ==,
                   GIMP_MAINIMAGE_CHANNEL1_WIDTH);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (channel)),
                   ==,
                   GIMP_MAINIMAGE_CHANNEL1_HEIGHT);
  g_assert (memcmp (&expected_channel_color,
                    &actual_channel_color,
                    sizeof (GimpRGB)) == 0);

  /* Selection, if the image contains unusual stuff it contains a
   * floating select, and when floating a selection, the selection
   * mask is cleared, so don't test for the presence of the selection
   * mask in that case
   */
  if (! with_unusual_stuff)
    {
      selection = gimp_image_get_mask (image);
      gimp_channel_bounds (selection, &x1, &y1, &x2, &y2);
      w = x2 - x1;
      h = y2 - y1;
      g_assert_cmpint (x1,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_X);
      g_assert_cmpint (y1,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_Y);
      g_assert_cmpint (w,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_W);
      g_assert_cmpint (h,
                       ==,
                       GIMP_MAINIMAGE_SELECTION_H);
    }

  /* Vectors 1 */
  gimp_assert_vectors (image,
                       GIMP_MAINIMAGE_VECTORS1_NAME,
                       vectors1_coords,
                       G_N_ELEMENTS (vectors1_coords),
                       ! compat_paths /*visible*/);

  /* Vectors 2 (always visible FALSE) */
  gimp_assert_vectors (image,
                       GIMP_MAINIMAGE_VECTORS2_NAME,
                       vectors2_coords,
                       G_N_ELEMENTS (vectors2_coords),
                       FALSE /*visible*/);

  if (with_unusual_stuff)
    g_assert (gimp_image_get_floating_selection (image) != NULL);
  else /* if (! with_unusual_stuff) */
    g_assert (gimp_image_get_floating_selection (image) == NULL);
}


/**
 * main:
 * @argc:
 * @argv:
 *
 * These tests intend to
 *
 *  - Make sure that we are backwards compatible with files created by
 *    older version of GIMP, i.e. that we can load files from earlier
 *    version of GIMP
 *
 *  - Make sure that the information put into a #GimpImage is not lost
 *    when the #GimpImage is written to a file and then read again
 **/
int
main (int    argc,
      char **argv)
{
  g_type_init ();
  gtk_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  gimp_test_utils_set_gimp2_directory ("gimpdir");

  /* We share the same application instance across all tests. We need
   * the GUI variant for the file procs
   */
  gimp = gimp_init_for_gui_testing (TRUE, FALSE);

  /* Setup the tests */
  g_test_add ("/gimp-xcf/write-and-read-gimp-2-6",
              GimpTestFixture,
              NULL,
              NULL,
              gimp_write_and_read_gimp_2_6_format,
              NULL);
  g_test_add ("/gimp-xcf/write-and-read-gimp-2-6-unusual",
              GimpTestFixture,
              NULL,
              NULL,
              gimp_write_and_read_gimp_2_6_format_unusual,
              NULL);
  g_test_add ("/gimp-xcf/load-gimp-2-6-file",
              GimpTestFixture,
              NULL,
              NULL,
              gimp_load_gimp_2_6_file,
              NULL);

  /* Exit so we don't break script-fu plug-in wire */
  gimp_exit (gimp, TRUE);

  /* Run the tests and return status */
  return g_test_run ();
}
