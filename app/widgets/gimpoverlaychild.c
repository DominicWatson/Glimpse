/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpoverlaychild.c
 * Copyright (C) 2009 Michael Natterer <mitch@gimp.org>
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

#undef GSEAL_ENABLE

#include <gtk/gtk.h>

#include <libgimpmath/gimpmath.h>

#include "widgets-types.h"

#include "gimpoverlaybox.h"
#include "gimpoverlaychild.h"


/*  local function prototypes  */

static void   gimp_overlay_child_transform_bounds (GimpOverlayChild *child,
                                                   GdkRectangle     *bounds_child,
                                                   GdkRectangle     *bounds_box);
static void   gimp_overlay_child_from_embedder    (GdkWindow        *child_window,
                                                   gdouble           box_x,
                                                   gdouble           box_y,
                                                   gdouble          *child_x,
                                                   gdouble          *child_y,
                                                   GimpOverlayChild *child);
static void   gimp_overlay_child_to_embedder      (GdkWindow        *child_window,
                                                   gdouble           child_x,
                                                   gdouble           child_y,
                                                   gdouble          *box_x,
                                                   gdouble          *box_y,
                                                   GimpOverlayChild *child);


/*  public functions  */

GimpOverlayChild *
gimp_overlay_child_new (GimpOverlayBox *box,
                        GtkWidget      *widget,
                        gdouble         xalign,
                        gdouble         yalign,
                        gdouble         angle,
                        gdouble         opacity)
{
  GimpOverlayChild *child;

  g_return_val_if_fail (GIMP_IS_OVERLAY_BOX (box), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  child = g_slice_new0 (GimpOverlayChild);

  child->widget  = widget;
  child->xalign  = CLAMP (xalign, 0.0, 1.0);
  child->yalign  = CLAMP (yalign, 0.0, 1.0);
  child->angle   = angle;
  child->opacity = CLAMP (opacity, 0.0, 1.0);

  cairo_matrix_init_identity (&child->matrix);

  if (GTK_WIDGET_REALIZED (box))
    gimp_overlay_child_realize (box, child);

  gtk_widget_set_parent (widget, GTK_WIDGET (box));

  return child;
}

void
gimp_overlay_child_free (GimpOverlayBox   *box,
                         GimpOverlayChild *child)
{
  g_return_if_fail (GIMP_IS_OVERLAY_BOX (box));
  g_return_if_fail (child != NULL);

  gtk_widget_unparent (child->widget);

  if (GTK_WIDGET_REALIZED (box))
    gimp_overlay_child_unrealize (box, child);

  g_slice_free (GimpOverlayChild, child);
}

GimpOverlayChild *
gimp_overlay_child_find (GimpOverlayBox *box,
                         GtkWidget      *widget)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_OVERLAY_BOX (box), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (box),
                        NULL);

  for (list = box->children; list; list = g_list_next (list))
    {
      GimpOverlayChild *child = list->data;

      if (child->widget == widget)
        return child;
    }

  return NULL;
}

void
gimp_overlay_child_realize (GimpOverlayBox   *box,
                            GimpOverlayChild *child)
{
  GtkWidget     *widget;
  GtkAllocation  child_allocation;
  GdkWindowAttr  attributes;
  gint           attributes_mask;

  g_return_if_fail (GIMP_IS_OVERLAY_BOX (box));
  g_return_if_fail (child != NULL);

  widget = GTK_WIDGET (box);

  gtk_widget_get_allocation (child->widget, &child_allocation);

  if (gtk_widget_get_visible (child->widget))
    {
      attributes.width  = child_allocation.width;
      attributes.height = child_allocation.height;
    }
  else
    {
      attributes.width  = 1;
      attributes.height = 1;
    }

  attributes.x           = child_allocation.x;
  attributes.y           = child_allocation.y;
  attributes.window_type = GDK_WINDOW_OFFSCREEN;
  attributes.wclass      = GDK_INPUT_OUTPUT;
  attributes.visual      = gtk_widget_get_visual (widget);
  attributes.colormap    = gtk_widget_get_colormap (widget);
  attributes.event_mask  = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  child->window = gdk_window_new (gtk_widget_get_root_window (widget),
                                  &attributes, attributes_mask);
  gdk_window_set_user_data (child->window, widget);
  gtk_widget_set_parent_window (child->widget, child->window);
  gdk_offscreen_window_set_embedder (child->window,
                                     gtk_widget_get_window (widget));

  g_signal_connect (child->window, "from-embedder",
                    G_CALLBACK (gimp_overlay_child_from_embedder),
                    child);
  g_signal_connect (child->window, "to-embedder",
                    G_CALLBACK (gimp_overlay_child_to_embedder),
                    child);

  gtk_style_set_background (gtk_widget_get_style (widget),
                            child->window, GTK_STATE_NORMAL);
  gdk_window_show (child->window);
}

void
gimp_overlay_child_unrealize (GimpOverlayBox   *box,
                              GimpOverlayChild *child)
{
  g_return_if_fail (GIMP_IS_OVERLAY_BOX (box));
  g_return_if_fail (child != NULL);

  gdk_window_set_user_data (child->window, NULL);
  gdk_window_destroy (child->window);
  child->window = NULL;
}

void
gimp_overlay_child_size_request (GimpOverlayBox   *box,
                                 GimpOverlayChild *child)
{
  GtkRequisition child_requisition;

  g_return_if_fail (GIMP_IS_OVERLAY_BOX (box));
  g_return_if_fail (child != NULL);

  gtk_widget_size_request (child->widget, &child_requisition);
}

void
gimp_overlay_child_size_allocate (GimpOverlayBox   *box,
                                  GimpOverlayChild *child)
{
  GtkWidget      *widget;
  GtkAllocation   allocation;
  gint            border;
  GtkRequisition  child_requisition;
  GtkAllocation   child_allocation;
  GdkRectangle    bounds;
  gint            available_width;
  gint            available_height;
  gint            x;
  gint            y;

  g_return_if_fail (GIMP_IS_OVERLAY_BOX (box));
  g_return_if_fail (child != NULL);

  widget = GTK_WIDGET (box);

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_get_child_requisition (child->widget, &child_requisition);

  child_allocation.x      = 0;
  child_allocation.y      = 0;
  child_allocation.width  = child_requisition.width;
  child_allocation.height = child_requisition.height;

  gtk_widget_size_allocate (child->widget, &child_allocation);

  gtk_widget_get_allocation (child->widget, &child_allocation);

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (child->window,
                            child_allocation.x,
                            child_allocation.y,
                            child_allocation.width,
                            child_allocation.height);

  cairo_matrix_init_identity (&child->matrix);

  /* local transform */
  cairo_matrix_rotate (&child->matrix, child->angle);

  gimp_overlay_child_transform_bounds (child, &child_allocation, &bounds);

  border = gtk_container_get_border_width (GTK_CONTAINER (box));

  available_width  = allocation.width  - 2 * border;
  available_height = allocation.height - 2 * border;

  x = border;
  y = border;

  if (available_width > bounds.width)
    x += child->xalign * (available_width - bounds.width) - bounds.x;

  if (available_height > bounds.height)
    y += child->yalign * (available_height - bounds.height) - bounds.y;

  cairo_matrix_init_translate (&child->matrix, x, y);

  /* local transform */
  cairo_matrix_rotate (&child->matrix, child->angle);
}

gboolean
gimp_overlay_child_expose (GimpOverlayBox   *box,
                           GimpOverlayChild *child,
                           GdkEventExpose   *event)
{
  GtkWidget *widget;

  g_return_val_if_fail (GIMP_IS_OVERLAY_BOX (box), FALSE);
  g_return_val_if_fail (child != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  widget = GTK_WIDGET (box);

  if (event->window == gtk_widget_get_window (widget))
    {
      GtkAllocation child_allocation;
      GdkRectangle  bounds;

      gtk_widget_get_allocation (child->widget, &child_allocation);

      gimp_overlay_child_transform_bounds (child, &child_allocation, &bounds);

      if (gtk_widget_get_visible (child->widget) &&
          gdk_rectangle_intersect (&event->area, &bounds, NULL))
        {
          GdkPixmap *pixmap = gdk_offscreen_window_get_pixmap (child->window);
          cairo_t   *cr     = gdk_cairo_create (gtk_widget_get_window (widget));

          cairo_transform (cr, &child->matrix);
          gdk_cairo_set_source_pixmap (cr, pixmap, 0, 0);
          cairo_paint_with_alpha (cr, child->opacity);
          cairo_destroy (cr);
        }
    }
  else if (event->window == child->window)
    {
      gtk_paint_flat_box (gtk_widget_get_style (widget),
                          event->window,
                          GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                          &event->area, widget, NULL,
                          0, 0, -1, -1);

      gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                      child->widget,
                                      event);

      return TRUE;
    }

  return FALSE;
}

gboolean
gimp_overlay_child_damage (GimpOverlayBox   *box,
                           GimpOverlayChild *child,
                           GdkEventExpose   *event)
{
  GtkWidget *widget;

  g_return_val_if_fail (GIMP_IS_OVERLAY_BOX (box), FALSE);
  g_return_val_if_fail (child != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  widget = GTK_WIDGET (box);

  if (event->window == child->window)
    {
      GdkRectangle bounds;

      gimp_overlay_child_transform_bounds (child, &event->area, &bounds);

      gdk_window_invalidate_rect (gtk_widget_get_window (widget),
                                  &bounds, FALSE);

      return TRUE;
    }

  return FALSE;
}

void
gimp_overlay_child_invalidate (GimpOverlayBox   *box,
                               GimpOverlayChild *child)
{
  GdkWindow *window;

  g_return_if_fail (GIMP_IS_OVERLAY_BOX (box));
  g_return_if_fail (child != NULL);

  window = gtk_widget_get_window (GTK_WIDGET (box));

  if (window && gtk_widget_get_visible (child->widget))
    {
      GtkAllocation child_allocation;
      GdkRectangle  bounds;

      gtk_widget_get_allocation (child->widget, &child_allocation);

      gimp_overlay_child_transform_bounds (child, &child_allocation,
                                           &bounds);

      gdk_window_invalidate_rect (window, &bounds, FALSE);
    }
}

gboolean
gimp_overlay_child_pick (GimpOverlayBox   *box,
                         GimpOverlayChild *child,
                         gdouble           box_x,
                         gdouble           box_y)
{
  GtkAllocation child_allocation;
  gdouble       child_x;
  gdouble       child_y;

  g_return_val_if_fail (GIMP_IS_OVERLAY_BOX (box), FALSE);
  g_return_val_if_fail (child != NULL, FALSE);

  gimp_overlay_child_from_embedder (child->window,
                                    box_x, box_y,
                                    &child_x, &child_y,
                                    child);

  gtk_widget_get_allocation (child->widget, &child_allocation);

  if (child_x >= 0                      &&
      child_x <  child_allocation.width &&
      child_y >= 0                      &&
      child_y <  child_allocation.height)
    {
      return TRUE;
    }

  return FALSE;
}


/*  private functions  */

static void
gimp_overlay_child_transform_bounds (GimpOverlayChild *child,
                                     GdkRectangle     *bounds_child,
                                     GdkRectangle     *bounds_box)
{
  gdouble x1, x2, x3, x4;
  gdouble y1, y2, y3, y4;

  x1 = bounds_child->x;
  y1 = bounds_child->y;

  x2 = bounds_child->x + bounds_child->width;
  y2 = bounds_child->y;

  x3 = bounds_child->x;
  y3 = bounds_child->y + bounds_child->height;

  x4 = bounds_child->x + bounds_child->width;
  y4 = bounds_child->y + bounds_child->height;

  cairo_matrix_transform_point (&child->matrix, &x1, &y1);
  cairo_matrix_transform_point (&child->matrix, &x2, &y2);
  cairo_matrix_transform_point (&child->matrix, &x3, &y3);
  cairo_matrix_transform_point (&child->matrix, &x4, &y4);

#define MIN4(a,b,c,d) MIN(MIN((a),(b)),MIN((c),(d)))
#define MAX4(a,b,c,d) MAX(MAX((a),(b)),MAX((c),(d)))

  bounds_box->x      = (gint) floor (MIN4 (x1, x2, x3, x4));
  bounds_box->y      = (gint) floor (MIN4 (y1, y2, y3, y4));
  bounds_box->width  = (gint) ceil (MAX4 (x1, x2, x3, x4)) - bounds_box->x;
  bounds_box->height = (gint) ceil (MAX4 (y1, y2, y3, y4)) - bounds_box->y;
}

static void
gimp_overlay_child_from_embedder (GdkWindow        *child_window,
                                  gdouble           box_x,
                                  gdouble           box_y,
                                  gdouble          *child_x,
                                  gdouble          *child_y,
                                  GimpOverlayChild *child)
{
  cairo_matrix_t inverse = child->matrix;

  *child_x = box_x;
  *child_y = box_y;

  cairo_matrix_invert (&inverse);
  cairo_matrix_transform_point (&inverse, child_x, child_y);
}

static void
gimp_overlay_child_to_embedder (GdkWindow        *child_window,
                                gdouble           child_x,
                                gdouble           child_y,
                                gdouble          *box_x,
                                gdouble          *box_y,
                                GimpOverlayChild *child)
{
  *box_x = child_x;
  *box_y = child_y;

  cairo_matrix_transform_point (&child->matrix, box_x, box_y);
}
