/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include <stdlib.h>

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "config/gimpdisplayconfig.h"

#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpunit.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimpviewabledialog.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-scale.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpdisplayshell-title.h"

#include "gimp-intl.h"


#define SCALE_TIMEOUT 1

#define SCALE_EPSILON 0.0001

#define SCALE_EQUALS(a,b) (fabs ((a) - (b)) < SCALE_EPSILON)


typedef struct
{
  GimpDisplayShell *shell;
  GimpZoomModel    *model;
  GtkObject        *scale_adj;
  GtkObject        *num_adj;
  GtkObject        *denom_adj;
} ScaleDialogData;


/*  local function prototypes  */

static void gimp_display_shell_scale_dialog_response (GtkWidget        *widget,
                                                      gint              response_id,
                                                      ScaleDialogData  *dialog);
static void gimp_display_shell_scale_dialog_free     (ScaleDialogData  *dialog);

static void    update_zoom_values                    (GtkAdjustment    *adj,
                                                      ScaleDialogData  *dialog);
static gdouble img2real                              (GimpDisplayShell *shell,
                                                      gboolean          xdir,
                                                      gdouble           a);


/*  public functions  */

/**
 * gimp_display_shell_scale_setup:
 * @shell: the #GimpDisplayShell
 *
 * Prepares the display for drawing the image at current scale and offset.
 * This preparation involves, for example, setting up scrollbars and rulers.
 **/
void
gimp_display_shell_scale_setup (GimpDisplayShell *shell)
{
  GimpImage *image;
  gfloat     sw, sh;
  gint       image_width;
  gint       image_height;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! shell->display)
    return;

  image = shell->display->image;

  if (image)
    {
      image_width  = gimp_image_get_width  (image);
      image_height = gimp_image_get_height (image);

      sw = SCALEX (shell, image_width);
      sh = SCALEY (shell, image_height);
    }
  else
    {
      image_width  = shell->disp_width;
      image_height = shell->disp_height;

      sw = image_width;
      sh = image_height;
    }


  /* Horizontal scrollbar */

  shell->hsbdata->value          = shell->offset_x;
  shell->hsbdata->page_size      = shell->disp_width;
  shell->hsbdata->page_increment = shell->disp_width / 2;
  shell->hsbdata->step_increment = shell->scale_x;

  gimp_display_shell_setup_hscrollbar_with_value (shell, shell->offset_x);

  gtk_adjustment_changed (shell->hsbdata);


  /* Vertcal scrollbar */

  shell->vsbdata->value          = shell->offset_y;
  shell->vsbdata->page_size      = shell->disp_height;
  shell->vsbdata->page_increment = shell->disp_height / 2;
  shell->vsbdata->step_increment = shell->scale_y;

  gimp_display_shell_setup_vscrollbar_with_value (shell, shell->offset_y);

  gtk_adjustment_changed (shell->vsbdata);


  /* Setup rulers */
  {
    gdouble horizontal_lower;
    gdouble horizontal_upper;
    gdouble horizontal_max_size;
    gdouble vertical_lower;
    gdouble vertical_upper;
    gdouble vertical_max_size;
    gint    scaled_image_viewport_offset_x;
    gint    scaled_image_viewport_offset_y;


    /* Initialize values */

    horizontal_lower = 0;
    vertical_lower   = 0;

    if (image)
      {
        horizontal_upper    = img2real (shell, TRUE, FUNSCALEX (shell, shell->disp_width));
        horizontal_max_size = img2real (shell, TRUE, MAX (image_width, image_height));

        vertical_upper      = img2real (shell, FALSE, FUNSCALEY (shell, shell->disp_height));
        vertical_max_size   = img2real (shell, FALSE, MAX (image_width, image_height));
      }
    else
      {
        horizontal_upper    = image_width;
        horizontal_max_size = MAX (image_width, image_height);

        vertical_upper      = image_height;
        vertical_max_size   = MAX (image_width, image_height);
      }


    /* Adjust due to scrolling */

    gimp_display_shell_get_scaled_image_viewport_offset (shell,
                                                         &scaled_image_viewport_offset_x,
                                                         &scaled_image_viewport_offset_y);

    horizontal_lower -= img2real (shell, TRUE,
                                  FUNSCALEX (shell, (gdouble) scaled_image_viewport_offset_x));
    horizontal_upper -= img2real (shell, TRUE,
                                  FUNSCALEX (shell, (gdouble) scaled_image_viewport_offset_x));

    vertical_lower   -= img2real (shell, FALSE,
                                  FUNSCALEY (shell, (gdouble) scaled_image_viewport_offset_y));
    vertical_upper   -= img2real (shell, FALSE,
                                  FUNSCALEY (shell, (gdouble) scaled_image_viewport_offset_y));


    /* Finally setup the actual rulers */

    gimp_ruler_set_range (GIMP_RULER (shell->hrule),
                          horizontal_lower,
                          horizontal_upper,
                          horizontal_max_size);

    gimp_ruler_set_unit  (GIMP_RULER (shell->hrule),
                          shell->unit);

    gimp_ruler_set_range (GIMP_RULER (shell->vrule),
                          vertical_lower,
                          vertical_upper,
                          vertical_max_size);

    gimp_ruler_set_unit  (GIMP_RULER (shell->vrule),
                          shell->unit);
  }
}

/**
 * gimp_display_shell_scale_revert:
 * @shell:     the #GimpDisplayShell
 *
 * Reverts the display to the previously used scale. If no previous
 * scale exist, then the call does nothing.
 *
 * Return value: %TRUE if the scale was reverted, otherwise %FALSE.
 **/
gboolean
gimp_display_shell_scale_revert (GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  /* don't bother if no scale has been set */
  if (shell->last_scale < SCALE_EPSILON)
    return FALSE;

  shell->last_scale_time = 0;

  gimp_display_shell_scale_by_values (shell,
                                      shell->last_scale,
                                      shell->last_offset_x,
                                      shell->last_offset_y,
                                      FALSE);   /* don't resize the window */

  return TRUE;
}

/**
 * gimp_display_shell_scale_can_revert:
 * @shell: the #GimpDisplayShell
 *
 * Return value: %TRUE if a previous display scale exists, otherwise %FALSE.
 **/
gboolean
gimp_display_shell_scale_can_revert (GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return (shell->last_scale > SCALE_EPSILON);
}

/**
 * gimp_display_shell_scale_set_dot_for_dot:
 * @shell:        the #GimpDisplayShell
 * @dot_for_dot:  whether "Dot for Dot" should be enabled
 *
 * If @dot_for_dot is set to %TRUE then the "Dot for Dot" mode (where image and
 * screen pixels are of the same size) is activated. Dually, the mode is
 * disabled if @dot_for_dot is %FALSE.
 **/
void
gimp_display_shell_scale_set_dot_for_dot (GimpDisplayShell *shell,
                                          gboolean          dot_for_dot)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (dot_for_dot != shell->dot_for_dot)
    {
      /* freeze the active tool */
      gimp_display_shell_pause (shell);

      shell->dot_for_dot = dot_for_dot;

      gimp_display_shell_scale_changed (shell);

      gimp_display_shell_scale_resize (shell,
                                       shell->display->config->resize_windows_on_zoom,
                                       FALSE);

      /* re-enable the active tool */
      gimp_display_shell_resume (shell);
    }
}

/**
 * gimp_display_shell_scale:
 * @shell:     the #GimpDisplayShell
 * @zoom_type: whether to zoom in, our or to a specific scale
 * @scale:     ignored unless @zoom_type == %GIMP_ZOOM_TO
 *
 * This function calls gimp_display_shell_scale_to(). It tries to be
 * smart whether to use the position of the mouse pointer or the
 * center of the display as coordinates.
 **/
void
gimp_display_shell_scale (GimpDisplayShell *shell,
                          GimpZoomType      zoom_type,
                          gdouble           new_scale)
{
  GdkEvent *event;
  gint      x, y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (shell->canvas != NULL);

  if (zoom_type == GIMP_ZOOM_TO &&
      SCALE_EQUALS (new_scale, gimp_zoom_model_get_factor (shell->zoom)))
    return;

  x = shell->disp_width  / 2;
  y = shell->disp_height / 2;

  /*  Center on the mouse position instead of the display center,
   *  if one of the following conditions is fulfilled:
   *
   *   (1) there's no current event (the action was triggered by an
   *       input controller)
   *   (2) the event originates from the canvas (a scroll event)
   *   (3) the event originates from the shell (a key press event)
   *
   *  Basically the only situation where we don't want to center on
   *  mouse position is if the action is being called from a menu.
   */

  event = gtk_get_current_event ();

  if (! event ||
      gtk_get_event_widget (event) == shell->canvas ||
      gtk_get_event_widget (event) == GTK_WIDGET (shell))
    {
      gtk_widget_get_pointer (shell->canvas, &x, &y);
    }

  gimp_display_shell_scale_to (shell, zoom_type, new_scale, x, y);
}

/**
 * gimp_display_shell_scale_to:
 * @shell:     the #GimpDisplayShell
 * @zoom_type: whether to zoom in, out or to a specified scale
 * @scale:     ignored unless @zoom_type == %GIMP_ZOOM_TO
 * @x:         x screen coordinate
 * @y:         y screen coordinate
 *
 * This function changes the scale (zoom ratio) of the display shell.
 * It either zooms in / out one step (%GIMP_ZOOM_IN / %GIMP_ZOOM_OUT)
 * or sets the scale to the zoom ratio passed as @scale (%GIMP_ZOOM_TO).
 *
 * The display offsets are adjusted so that the point specified by @x
 * and @y doesn't change it's position on screen (if possible). You
 * would typically pass either the display center or the mouse
 * position here.
 **/
void
gimp_display_shell_scale_to (GimpDisplayShell *shell,
                             GimpZoomType      zoom_type,
                             gdouble           scale,
                             gdouble           x,
                             gdouble           y)
{
  gdouble current;
  gdouble offset_x;
  gdouble offset_y;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (! shell->display)
    return;

  current = gimp_zoom_model_get_factor (shell->zoom);

  offset_x = shell->offset_x + x;
  offset_y = shell->offset_y + y;

  offset_x /= current;
  offset_y /= current;

  if (zoom_type != GIMP_ZOOM_TO)
    scale = gimp_zoom_model_zoom_step (zoom_type, current);

  offset_x *= scale;
  offset_y *= scale;

  gimp_display_shell_scale_by_values (shell, scale,
                                      offset_x - x, offset_y - y,
                                      shell->display->config->resize_windows_on_zoom);
}

/**
 * gimp_display_shell_scale_fit_in:
 * @shell: the #GimpDisplayShell
 *
 * Sets the scale such that the entire image precisely fits in the display
 * area.
 **/
void
gimp_display_shell_scale_fit_in (GimpDisplayShell *shell)
{
  GimpImage *image;
  gint       image_width;
  gint       image_height;
  gdouble    xres;
  gdouble    yres;
  gdouble    zoom_factor;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = shell->display->image;

  image_width  = gimp_image_get_width  (image);
  image_height = gimp_image_get_height (image);

  gimp_image_get_resolution (image, &xres, &yres);

  if (! shell->dot_for_dot)
    {
      image_width  = ROUND (image_width  * shell->monitor_xres / xres);
      image_height = ROUND (image_height * shell->monitor_yres / yres);
    }

  zoom_factor = MIN ((gdouble) shell->disp_width  / (gdouble) image_width,
                     (gdouble) shell->disp_height / (gdouble) image_height);

  gimp_display_shell_scale (shell, GIMP_ZOOM_TO, zoom_factor);
}

/**
 * gimp_display_shell_scale_fill:
 * @shell: the #GimpDisplayShell
 *
 * Sets the scale such that the entire display area is precisely filled by the
 * image.
 **/
void
gimp_display_shell_scale_fill (GimpDisplayShell *shell)
{
  GimpImage *image;
  gint       image_width;
  gint       image_height;
  gdouble    xres;
  gdouble    yres;
  gdouble    zoom_factor;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  image = shell->display->image;

  image_width  = gimp_image_get_width  (image);
  image_height = gimp_image_get_height (image);

  gimp_image_get_resolution (image, &xres, &yres);

  if (! shell->dot_for_dot)
    {
      image_width  = ROUND (image_width  * shell->monitor_xres / xres);
      image_height = ROUND (image_height * shell->monitor_yres / yres);
    }

  zoom_factor = MAX ((gdouble) shell->disp_width  / (gdouble) image_width,
                     (gdouble) shell->disp_height / (gdouble) image_height);

  gimp_display_shell_scale (shell, GIMP_ZOOM_TO, zoom_factor);
}

/**
 * gimp_display_shell_scale_by_values:
 * @shell:         the #GimpDisplayShell
 * @scale:         the new scale
 * @offset_x:      the new X offset
 * @offset_y:      the new Y offset
 * @resize_window: whether the display window should be resized
 *
 * Directly sets the image scale and image offsets used by the display. If
 * @resize_window is %TRUE then the display window is resized to better
 * accomodate the image, see gimp_display_shell_shrink_wrap().
 **/
void
gimp_display_shell_scale_by_values (GimpDisplayShell *shell,
                                    gdouble           scale,
                                    gint              offset_x,
                                    gint              offset_y,
                                    gboolean          resize_window)
{
  guint now;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  /*  Abort early if the values are all setup already. We don't
   *  want to inadvertently resize the window (bug #164281).
   */
  if (SCALE_EQUALS (gimp_zoom_model_get_factor (shell->zoom), scale) &&
      shell->offset_x == offset_x &&
      shell->offset_y == offset_y)
    return;

  /* remember the current scale and offsets to allow reverting the scaling */

  now = time (NULL);

  if (now - shell->last_scale_time > SCALE_TIMEOUT)
    {
      shell->last_scale    = gimp_zoom_model_get_factor (shell->zoom);
      shell->last_offset_x = shell->offset_x;
      shell->last_offset_y = shell->offset_y;
    }

  shell->last_scale_time = now;

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO, scale);

  shell->offset_x = offset_x;
  shell->offset_y = offset_y;

  gimp_display_shell_scale_resize (shell, resize_window, FALSE);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

/**
 * gimp_display_shell_scale_shrink_wrap:
 * @shell: the #GimpDisplayShell
 *
 * Convenience function with the same functionality as
 * gimp_display_shell_scale_resize(@shell, TRUE, grow_only).
 **/
void
gimp_display_shell_scale_shrink_wrap (GimpDisplayShell *shell,
                                      gboolean          grow_only)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  gimp_display_shell_scale_resize (shell, TRUE, grow_only);
}

/**
 * gimp_display_shell_scale_resize:
 * @shell:          the #GimpDisplayShell
 * @resize_window:  whether the display window should be resized
 * @grow_only:      whether shrinking of the window is allowed or not
 *
 * Function commonly called after a change in display scale to make the changes
 * visible to the user. If @resize_window is %TRUE then the display window is
 * resized to accomodate the display image as per
 * gimp_display_shell_shrink_wrap().
 **/
void
gimp_display_shell_scale_resize (GimpDisplayShell *shell,
                                 gboolean          resize_window,
                                 gboolean          grow_only)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  /* freeze the active tool */
  gimp_display_shell_pause (shell);

  if (resize_window)
    gimp_display_shell_shrink_wrap (shell, grow_only);

  gimp_display_shell_scroll_clamp_offsets (shell);
  gimp_display_shell_scale_setup (shell);
  gimp_display_shell_scaled (shell);

  gimp_display_shell_expose_full (shell);

  /* re-enable the active tool */
  gimp_display_shell_resume (shell);
}

void
gimp_display_shell_set_initial_scale (GimpDisplayShell *shell,
                                      gdouble           scale,
                                      gint             *display_width,
                                      gint             *display_height)
{
  GdkScreen *screen;
  gint       image_width;
  gint       image_height;
  gint       shell_width;
  gint       shell_height;
  gint       screen_width;
  gint       screen_height;
  gdouble    new_scale;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  screen = gtk_widget_get_screen (GTK_WIDGET (shell));

  image_width  = gimp_image_get_width  (shell->display->image);
  image_height = gimp_image_get_height (shell->display->image);

  screen_width  = gdk_screen_get_width (screen)  * 0.75;
  screen_height = gdk_screen_get_height (screen) * 0.75;

  shell_width  = SCALEX (shell, image_width);
  shell_height = SCALEY (shell, image_height);

  gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO, scale);

  if (shell->display->config->initial_zoom_to_fit)
    {
      /*  Limit to the size of the screen...  */
      if (shell_width > screen_width || shell_height > screen_height)
        {
          gdouble current = gimp_zoom_model_get_factor (shell->zoom);

          new_scale = current * MIN (((gdouble) screen_height) / shell_height,
                                     ((gdouble) screen_width)  / shell_width);

          new_scale = gimp_zoom_model_zoom_step (GIMP_ZOOM_OUT, new_scale);

          /*  Since zooming out might skip a zoom step we zoom in
           *  again and test if we are small enough.
           */
          gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO,
                                gimp_zoom_model_zoom_step (GIMP_ZOOM_IN,
                                                           new_scale));

          if (SCALEX (shell, image_width) > screen_width ||
              SCALEY (shell, image_height) > screen_height)
            gimp_zoom_model_zoom (shell->zoom, GIMP_ZOOM_TO, new_scale);

          shell_width  = SCALEX (shell, image_width);
          shell_height = SCALEY (shell, image_height);
        }
    }
  else
    {
      /*  Set up size like above, but do not zoom to fit. Useful when
       *  working on large images.
       */
      if (shell_width > screen_width)
        shell_width = screen_width;

      if (shell_height > screen_height)
        shell_height = screen_height;
    }

  if (display_width)
    *display_width = shell_width;

  if (display_height)
    *display_height = shell_height;
}

/**
 * gimp_display_shell_scale_dialog:
 * @shell: the #GimpDisplayShell
 *
 * Constructs and displays a dialog allowing the user to enter a custom display
 * scale.
 **/
void
gimp_display_shell_scale_dialog (GimpDisplayShell *shell)
{
  ScaleDialogData *data;
  GimpImage       *image;
  GtkWidget       *hbox;
  GtkWidget       *table;
  GtkWidget       *spin;
  GtkWidget       *label;
  gint             num, denom, row;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (shell->scale_dialog)
    {
      gtk_window_present (GTK_WINDOW (shell->scale_dialog));
      return;
    }

  if (SCALE_EQUALS (shell->other_scale, 0.0))
    {
      /* other_scale not yet initialized */
      shell->other_scale = gimp_zoom_model_get_factor (shell->zoom);
    }

  image = shell->display->image;

  data = g_slice_new (ScaleDialogData);

  data->shell = shell;
  data->model = g_object_new (GIMP_TYPE_ZOOM_MODEL,
                              "value", fabs (shell->other_scale),
                              NULL);

  shell->scale_dialog =
    gimp_viewable_dialog_new (GIMP_VIEWABLE (image),
                              gimp_get_user_context (shell->display->gimp),
                              _("Zoom Ratio"), "display_scale",
                              GTK_STOCK_ZOOM_100,
                              _("Select Zoom Ratio"),
                              GTK_WIDGET (shell),
                              gimp_standard_help_func,
                              GIMP_HELP_VIEW_ZOOM_OTHER,

                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              GTK_STOCK_OK,     GTK_RESPONSE_OK,

                              NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (shell->scale_dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  g_object_weak_ref (G_OBJECT (shell->scale_dialog),
                     (GWeakNotify) gimp_display_shell_scale_dialog_free, data);
  g_object_weak_ref (G_OBJECT (shell->scale_dialog),
                     (GWeakNotify) g_object_unref, data->model);

  g_object_add_weak_pointer (G_OBJECT (shell->scale_dialog),
                             (gpointer) &shell->scale_dialog);

  gtk_window_set_transient_for (GTK_WINDOW (shell->scale_dialog),
                                GTK_WINDOW (shell));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (shell->scale_dialog), TRUE);

  g_signal_connect (shell->scale_dialog, "response",
                    G_CALLBACK (gimp_display_shell_scale_dialog_response),
                    data);

  table = gtk_table_new (2, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (shell->scale_dialog)->vbox),
                     table);
  gtk_widget_show (table);

  row = 0;

  hbox = gtk_hbox_new (FALSE, 6);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("Zoom ratio:"), 0.0, 0.5,
                             hbox, 1, FALSE);

  gimp_zoom_model_get_fraction (data->model, &num, &denom);

  spin = gimp_spin_button_new (&data->num_adj,
                               num, 1, 256,
                               1, 8, 1, 1, 0);
  gtk_entry_set_activates_default (GTK_ENTRY (spin), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spin, TRUE, TRUE, 0);
  gtk_widget_show (spin);

  label = gtk_label_new (":");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  spin = gimp_spin_button_new (&data->denom_adj,
                               denom, 1, 256,
                               1, 8, 1, 1, 0);
  gtk_entry_set_activates_default (GTK_ENTRY (spin), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spin, TRUE, TRUE, 0);
  gtk_widget_show (spin);

  hbox = gtk_hbox_new (FALSE, 6);
  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("Zoom:"), 0.0, 0.5,
                             hbox, 1, FALSE);

  spin = gimp_spin_button_new (&data->scale_adj,
                               fabs (shell->other_scale) * 100,
                               100.0 / 256.0, 25600.0,
                               10, 50, 0, 1, 2);
  gtk_entry_set_activates_default (GTK_ENTRY (spin), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), spin, TRUE, TRUE, 0);
  gtk_widget_show (spin);

  label = gtk_label_new ("%");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  g_signal_connect (data->scale_adj, "value-changed",
                    G_CALLBACK (update_zoom_values), data);
  g_signal_connect (data->num_adj, "value-changed",
                    G_CALLBACK (update_zoom_values), data);
  g_signal_connect (data->denom_adj, "value-changed",
                    G_CALLBACK (update_zoom_values), data);

  gtk_widget_show (shell->scale_dialog);
}


/*  private functions  */

static void
gimp_display_shell_scale_dialog_response (GtkWidget       *widget,
                                          gint             response_id,
                                          ScaleDialogData *dialog)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      gdouble scale;

      scale = gtk_adjustment_get_value (GTK_ADJUSTMENT (dialog->scale_adj));

      gimp_display_shell_scale (dialog->shell, GIMP_ZOOM_TO, scale / 100.0);
    }
  else
    {
      /*  need to emit "scaled" to get the menu updated  */
      gimp_display_shell_scaled (dialog->shell);
    }

  dialog->shell->other_scale = - fabs (dialog->shell->other_scale);

  gtk_widget_destroy (dialog->shell->scale_dialog);
}

static void
gimp_display_shell_scale_dialog_free (ScaleDialogData *dialog)
{
  g_slice_free (ScaleDialogData, dialog);
}

static void
update_zoom_values (GtkAdjustment   *adj,
                    ScaleDialogData *dialog)
{
  gint    num, denom;
  gdouble scale;

  g_signal_handlers_block_by_func (GTK_ADJUSTMENT (dialog->scale_adj),
                                   G_CALLBACK (update_zoom_values),
                                   dialog);

  g_signal_handlers_block_by_func (GTK_ADJUSTMENT (dialog->num_adj),
                                   G_CALLBACK (update_zoom_values),
                                   dialog);

  g_signal_handlers_block_by_func (GTK_ADJUSTMENT (dialog->denom_adj),
                                   G_CALLBACK (update_zoom_values),
                                   dialog);

  if (GTK_OBJECT (adj) == dialog->scale_adj)
    {
      scale = gtk_adjustment_get_value (GTK_ADJUSTMENT (dialog->scale_adj));

      gimp_zoom_model_zoom (dialog->model, GIMP_ZOOM_TO, scale / 100.0);
      gimp_zoom_model_get_fraction (dialog->model, &num, &denom);

      gtk_adjustment_set_value (GTK_ADJUSTMENT (dialog->num_adj), num);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (dialog->denom_adj), denom);
    }
  else   /* fraction adjustments */
    {
      scale = (gtk_adjustment_get_value (GTK_ADJUSTMENT (dialog->num_adj)) /
               gtk_adjustment_get_value (GTK_ADJUSTMENT (dialog->denom_adj)));
      gtk_adjustment_set_value (GTK_ADJUSTMENT (dialog->scale_adj),
                                scale * 100);
    }

  g_signal_handlers_unblock_by_func (GTK_ADJUSTMENT (dialog->scale_adj),
                                     G_CALLBACK (update_zoom_values),
                                     dialog);

  g_signal_handlers_unblock_by_func (GTK_ADJUSTMENT (dialog->num_adj),
                                     G_CALLBACK (update_zoom_values),
                                     dialog);

  g_signal_handlers_unblock_by_func (GTK_ADJUSTMENT (dialog->denom_adj),
                                     G_CALLBACK (update_zoom_values),
                                     dialog);
}

/* scale image coord to realworld units (cm, inches, pixels)
 *
 * 27/Feb/1999 I tried inlining this, but the result was slightly
 * slower (poorer cache locality, probably) -- austin
 */
static gdouble
img2real (GimpDisplayShell *shell,
          gboolean          xdir,
          gdouble           len)
{
  gdouble xres;
  gdouble yres;
  gdouble res;

  if (shell->unit == GIMP_UNIT_PIXEL)
    return len;

  gimp_image_get_resolution (shell->display->image, &xres, &yres);

  if (xdir)
    res = xres;
  else
    res = yres;

  return len * _gimp_unit_get_factor (shell->display->gimp, shell->unit) / res;
}
