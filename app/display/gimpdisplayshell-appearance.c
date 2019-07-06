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
#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-grid.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-sample-points.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimprender.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimpcanvas.h"
#include "gimpdisplay.h"
#include "gimpdisplayoptions.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-appearance.h"
#include "gimpdisplayshell-selection.h"
#include "gimpimagewindow.h"
#include "gimpstatusbar.h"


/*  local function prototypes  */

static GimpDisplayOptions *
              appearance_get_options       (const GimpDisplayShell *shell);
static void   appearance_set_action_active (GimpDisplayShell       *shell,
                                            const gchar            *action,
                                            gboolean                active);
static void   appearance_set_action_color  (GimpDisplayShell       *shell,
                                            const gchar            *action,
                                            const GimpRGB          *color);


/*  public functions  */

void
gimp_display_shell_appearance_update (GimpDisplayShell *shell)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  gimp_display_shell_set_show_menubar       (shell,
                                             options->show_menubar);
  gimp_display_shell_set_show_statusbar     (shell,
                                             options->show_statusbar);

  gimp_display_shell_set_show_rulers        (shell,
                                             options->show_rulers);
  gimp_display_shell_set_show_scrollbars    (shell,
                                             options->show_scrollbars);
  gimp_display_shell_set_show_selection     (shell,
                                             options->show_selection);
  gimp_display_shell_set_show_layer         (shell,
                                             options->show_layer_boundary);
  gimp_display_shell_set_show_guides        (shell,
                                             options->show_guides);
  gimp_display_shell_set_show_grid          (shell,
                                             options->show_grid);
  gimp_display_shell_set_show_sample_points (shell,
                                             options->show_sample_points);
  gimp_display_shell_set_padding            (shell,
                                             options->padding_mode,
                                             &options->padding_color);
}

void
gimp_display_shell_set_show_menubar (GimpDisplayShell *shell,
                                     gboolean          show)
{
  GimpDisplayOptions *options;
  GtkWidget          *toplevel;
  GimpImageWindow    *window;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options  = appearance_get_options (shell);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  window   = GIMP_IMAGE_WINDOW (toplevel);

  g_object_set (options, "show-menubar", show, NULL);

  if (gimp_image_window_get_active_display (window) == shell->display &&
      window->menubar)
    {
      if (show)
        gtk_widget_show (window->menubar);
      else
        gtk_widget_hide (window->menubar);
    }

  appearance_set_action_active (shell, "view-show-menubar", show);
}

gboolean
gimp_display_shell_get_show_menubar (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_menubar;
}

void
gimp_display_shell_set_show_statusbar (GimpDisplayShell *shell,
                                       gboolean          show)
{
  GimpDisplayOptions *options;
  GtkWidget          *toplevel;
  GimpImageWindow    *window;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options  = appearance_get_options (shell);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  window   = GIMP_IMAGE_WINDOW (toplevel);

  g_object_set (options, "show-statusbar", show, NULL);

  if (gimp_image_window_get_active_display (window) == shell->display)
    {
      gimp_statusbar_set_visible (GIMP_STATUSBAR (window->statusbar), show);
    }

  appearance_set_action_active (shell, "view-show-statusbar", show);
}

gboolean
gimp_display_shell_get_show_statusbar (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_statusbar;
}

void
gimp_display_shell_set_show_rulers (GimpDisplayShell *shell,
                                    gboolean          show)
{
  GimpDisplayOptions *options;
  GtkTable           *table;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-rulers", show, NULL);

  table = GTK_TABLE (gtk_widget_get_parent (GTK_WIDGET (shell->canvas)));

  if (show)
    {
      gtk_widget_show (shell->origin);
      gtk_widget_show (shell->hrule);
      gtk_widget_show (shell->vrule);

      gtk_table_set_col_spacing (table, 0, 1);
      gtk_table_set_row_spacing (table, 0, 1);
    }
  else
    {
      gtk_widget_hide (shell->origin);
      gtk_widget_hide (shell->hrule);
      gtk_widget_hide (shell->vrule);

      gtk_table_set_col_spacing (table, 0, 0);
      gtk_table_set_row_spacing (table, 0, 0);
    }

  appearance_set_action_active (shell, "view-show-rulers", show);
}

gboolean
gimp_display_shell_get_show_rulers (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_rulers;
}

void
gimp_display_shell_set_show_scrollbars (GimpDisplayShell *shell,
                                        gboolean          show)
{
  GimpDisplayOptions *options;
  GtkWidget          *parent;
  GtkBox             *hbox;
  GtkBox             *vbox;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-scrollbars", show, NULL);

  parent = gtk_widget_get_parent (shell->vsb);
  hbox   = GTK_BOX (gtk_widget_get_parent (parent));

  parent = gtk_widget_get_parent (shell->hsb);
  vbox   = GTK_BOX (gtk_widget_get_parent (parent));

  if (show)
    {
      gtk_widget_show (shell->nav_ebox);
      gtk_widget_show (shell->hsb);
      gtk_widget_show (shell->vsb);
      gtk_widget_show (shell->quick_mask_button);
      gtk_widget_show (shell->zoom_button);

      gtk_box_set_spacing (hbox, 1);
      gtk_box_set_spacing (vbox, 1);
    }
  else
    {
      gtk_widget_hide (shell->nav_ebox);
      gtk_widget_hide (shell->hsb);
      gtk_widget_hide (shell->vsb);
      gtk_widget_hide (shell->quick_mask_button);
      gtk_widget_hide (shell->zoom_button);

      gtk_box_set_spacing (hbox, 0);
      gtk_box_set_spacing (vbox, 0);
    }

  appearance_set_action_active (shell, "view-show-scrollbars", show);
}

gboolean
gimp_display_shell_get_show_scrollbars (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_scrollbars;
}

void
gimp_display_shell_set_show_selection (GimpDisplayShell *shell,
                                       gboolean          show)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-selection", show, NULL);

  gimp_display_shell_selection_set_hidden (shell, ! show);

  appearance_set_action_active (shell, "view-show-selection", show);
}

gboolean
gimp_display_shell_get_show_selection (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_selection;
}

void
gimp_display_shell_set_show_layer (GimpDisplayShell *shell,
                                   gboolean          show)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-layer-boundary", show, NULL);

  gimp_display_shell_selection_layer_set_hidden (shell, ! show);

  appearance_set_action_active (shell, "view-show-layer-boundary", show);
}

gboolean
gimp_display_shell_get_show_layer (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_layer_boundary;
}

void
gimp_display_shell_set_show_transform (GimpDisplayShell *shell,
                                       gboolean          show)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  shell->show_transform_preview = show;
}

gboolean
gimp_display_shell_get_show_transform (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return shell->show_transform_preview;
}

void
gimp_display_shell_set_show_guides (GimpDisplayShell *shell,
                                    gboolean          show)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-guides", show, NULL);

  if (shell->display->image &&
      gimp_image_get_guides (shell->display->image))
    {
      gimp_display_shell_expose_full (shell);
    }

  appearance_set_action_active (shell, "view-show-guides", show);
}

gboolean
gimp_display_shell_get_show_guides (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_guides;
}

void
gimp_display_shell_set_show_grid (GimpDisplayShell *shell,
                                  gboolean          show)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-grid", show, NULL);

  if (shell->display->image &&
      gimp_image_get_grid (shell->display->image))
    {
      gimp_display_shell_expose_full (shell);
    }

  appearance_set_action_active (shell, "view-show-grid", show);
}

gboolean
gimp_display_shell_get_show_grid (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_grid;
}

void
gimp_display_shell_set_show_sample_points (GimpDisplayShell *shell,
                                           gboolean          show)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  g_object_set (options, "show-sample-points", show, NULL);

  if (shell->display->image &&
      gimp_image_get_sample_points (shell->display->image))
    {
      gimp_display_shell_expose_full (shell);
    }

  appearance_set_action_active (shell, "view-show-sample-points", show);
}

gboolean
gimp_display_shell_get_show_sample_points (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return appearance_get_options (shell)->show_sample_points;
}

void
gimp_display_shell_set_snap_to_grid (GimpDisplayShell *shell,
                                     gboolean          snap)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (snap != shell->snap_to_grid)
    {
      shell->snap_to_grid = snap ? TRUE : FALSE;

      appearance_set_action_active (shell, "view-snap-to-grid", snap);
    }
}

gboolean
gimp_display_shell_get_snap_to_grid (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return shell->snap_to_grid;
}

void
gimp_display_shell_set_snap_to_guides (GimpDisplayShell *shell,
                                       gboolean          snap)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (snap != shell->snap_to_guides)
    {
      shell->snap_to_guides = snap ? TRUE : FALSE;

      appearance_set_action_active (shell, "view-snap-to-guides", snap);
    }
}

gboolean
gimp_display_shell_get_snap_to_guides (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return shell->snap_to_guides;
}

void
gimp_display_shell_set_snap_to_canvas (GimpDisplayShell *shell,
                                       gboolean          snap)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (snap != shell->snap_to_canvas)
    {
      shell->snap_to_canvas = snap ? TRUE : FALSE;

      appearance_set_action_active (shell, "view-snap-to-canvas", snap);
    }
}

gboolean
gimp_display_shell_get_snap_to_canvas (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return shell->snap_to_canvas;
}

void
gimp_display_shell_set_snap_to_vectors (GimpDisplayShell *shell,
                                        gboolean          snap)
{
  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  if (snap != shell->snap_to_vectors)
    {
      shell->snap_to_vectors = snap ? TRUE : FALSE;

      appearance_set_action_active (shell, "view-snap-to-vectors", snap);
    }
}

gboolean
gimp_display_shell_get_snap_to_vectors (const GimpDisplayShell *shell)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), FALSE);

  return shell->snap_to_vectors;
}

void
gimp_display_shell_set_padding (GimpDisplayShell      *shell,
                                GimpCanvasPaddingMode  padding_mode,
                                const GimpRGB         *padding_color)
{
  GimpDisplayOptions *options;
  GimpRGB             color;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (padding_color != NULL);

  options = appearance_get_options (shell);
  color   = *padding_color;

  switch (padding_mode)
    {
    case GIMP_CANVAS_PADDING_MODE_DEFAULT:
      if (shell->canvas)
        {
          GtkStyle *style;

          gtk_widget_ensure_style (shell->canvas);

          style = gtk_widget_get_style (shell->canvas);

          gimp_rgb_set_gdk_color (&color, style->bg + GTK_STATE_NORMAL);
        }
      break;

    case GIMP_CANVAS_PADDING_MODE_LIGHT_CHECK:
      color = *gimp_render_light_check_color ();
      break;

    case GIMP_CANVAS_PADDING_MODE_DARK_CHECK:
      color = *gimp_render_dark_check_color ();
      break;

    case GIMP_CANVAS_PADDING_MODE_CUSTOM:
    case GIMP_CANVAS_PADDING_MODE_RESET:
      break;
    }

  g_object_set (options,
                "padding-mode",  padding_mode,
                "padding-color", &color,
                NULL);

  gimp_canvas_set_bg_color (GIMP_CANVAS (shell->canvas), &color);

  appearance_set_action_color (shell, "view-padding-color-menu",
                               &options->padding_color);

  gimp_display_shell_expose_full (shell);
}

void
gimp_display_shell_get_padding (const GimpDisplayShell *shell,
                                GimpCanvasPaddingMode  *padding_mode,
                                GimpRGB                *padding_color)
{
  GimpDisplayOptions *options;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));

  options = appearance_get_options (shell);

  if (padding_mode)
    *padding_mode = options->padding_mode;

  if (padding_color)
    *padding_color = options->padding_color;
}


/*  private functions  */

static GimpDisplayOptions *
appearance_get_options (const GimpDisplayShell *shell)
{
  if (shell->display->image)
    {
      GtkWidget       *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (shell));
      GimpImageWindow *window   = GIMP_IMAGE_WINDOW (toplevel);

      if (gimp_image_window_get_fullscreen (window))
        return shell->fullscreen_options;
      else
        return shell->options;
    }

  return shell->no_image_options;
}

static void
appearance_set_action_active (GimpDisplayShell *shell,
                              const gchar      *action,
                              gboolean          active)
{
  GtkWidget       *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  GimpImageWindow *window   = GIMP_IMAGE_WINDOW (toplevel);
  GimpContext     *context;

  if (gimp_image_window_get_active_display (window) == shell->display)
    {
      GimpActionGroup *action_group;

      action_group = gimp_ui_manager_get_action_group (window->menubar_manager,
                                                       "view");

      if (action_group)
        gimp_action_group_set_action_active (action_group, action, active);
    }

  context = gimp_get_user_context (shell->display->gimp);

  if (shell->display == gimp_context_get_display (context))
    {
      GimpActionGroup *action_group;

      action_group = gimp_ui_manager_get_action_group (shell->popup_manager,
                                                       "view");

      if (action_group)
        gimp_action_group_set_action_active (action_group, action, active);
    }
}

static void
appearance_set_action_color (GimpDisplayShell *shell,
                             const gchar      *action,
                             const GimpRGB    *color)
{
  GtkWidget       *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  GimpImageWindow *window   = GIMP_IMAGE_WINDOW (toplevel);
  GimpContext     *context;

  if (gimp_image_window_get_active_display (window) == shell->display)
    {
      GimpActionGroup *action_group;

      action_group = gimp_ui_manager_get_action_group (window->menubar_manager,
                                                       "view");

      if (action_group)
        gimp_action_group_set_action_color (action_group, action, color, FALSE);
    }

  context = gimp_get_user_context (shell->display->gimp);

  if (shell->display == gimp_context_get_display (context))
    {
      GimpActionGroup *action_group;

      action_group = gimp_ui_manager_get_action_group (shell->popup_manager,
                                                       "view");

      if (action_group)
        gimp_action_group_set_action_color (action_group, action, color, FALSE);
    }
}
