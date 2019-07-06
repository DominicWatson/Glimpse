/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimptooloverlay.c
 * Copyright (C) 2009  Michael Natterer <mitch@gimp.org>
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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpmarshal.h"
#include "core/gimptoolinfo.h"

#include "gimptooloverlay.h"


enum
{
  RESPONSE,
  CLOSE,
  LAST_SIGNAL
};


typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};


static void       gimp_tool_overlay_destroy       (GtkObject      *object);

static void       gimp_tool_overlay_size_request  (GtkWidget      *widget,
                                                   GtkRequisition *requisition);
static void       gimp_tool_overlay_size_allocate (GtkWidget      *widget,
                                                   GtkAllocation  *allocation);
static gboolean   gimp_tool_overlay_expose        (GtkWidget      *widget,
                                                   GdkEventExpose *eevent);

static void       gimp_tool_overlay_forall        (GtkContainer   *container,
                                                   gboolean        include_internals,
                                                   GtkCallback     callback,
                                                   gpointer        callback_data);

static ResponseData * get_response_data           (GtkWidget      *widget,
                                                   gboolean        create);


G_DEFINE_TYPE (GimpToolOverlay, gimp_tool_overlay, GTK_TYPE_BIN)

static guint signals[LAST_SIGNAL] = { 0, };

#define parent_class gimp_tool_overlay_parent_class


static void
gimp_tool_overlay_class_init (GimpToolOverlayClass *klass)
{
  GtkObjectClass    *gtk_object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass    *widget_class     = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class  = GTK_CONTAINER_CLASS (klass);

  gtk_object_class->destroy   = gimp_tool_overlay_destroy;

  widget_class->size_request  = gimp_tool_overlay_size_request;
  widget_class->size_allocate = gimp_tool_overlay_size_allocate;
  widget_class->expose_event  = gimp_tool_overlay_expose;

  container_class->forall     = gimp_tool_overlay_forall;

  signals[RESPONSE] =
    g_signal_new ("response",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GimpToolOverlayClass, response),
                  NULL, NULL,
                  gimp_marshal_VOID__INT,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  signals[CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GimpToolOverlayClass, close),
                  NULL, NULL,
                  gimp_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
gimp_tool_overlay_init (GimpToolOverlay *overlay)
{
  GtkWidget   *widget = GTK_WIDGET (overlay);
  GdkScreen   *screen = gtk_widget_get_screen (widget);
  GdkColormap *rgba   = gdk_screen_get_rgba_colormap (screen);

  if (rgba)
    gtk_widget_set_colormap (widget, rgba);

  gtk_widget_set_app_paintable (widget, TRUE);

  overlay->action_area = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (overlay->action_area),
                             GTK_BUTTONBOX_END);
  gtk_widget_set_parent (overlay->action_area, widget);
  gtk_widget_show (overlay->action_area);
}

static void
gimp_tool_overlay_destroy (GtkObject *object)
{
  GimpToolOverlay *overlay = GIMP_TOOL_OVERLAY (object);

  if (overlay->action_area)
    {
      gtk_widget_unparent (overlay->action_area);
      overlay->action_area = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gimp_tool_overlay_size_request (GtkWidget      *widget,
                                GtkRequisition *requisition)
{
  GtkContainer    *container = GTK_CONTAINER (widget);
  GimpToolOverlay *overlay   = GIMP_TOOL_OVERLAY (widget);
  GtkWidget       *child     = gtk_bin_get_child (GTK_BIN (widget));
  GtkRequisition   child_requisition;
  GtkRequisition   action_requisition;
  gint             border_width;

  border_width = gtk_container_get_border_width (container);

  requisition->width  = border_width * 2;
  requisition->height = border_width * 2;

  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_size_request (child, &child_requisition);
    }
  else
    {
      child_requisition.width  = 0;
      child_requisition.height = 0;
    }

  gtk_widget_size_request (overlay->action_area, &action_requisition);

  requisition->width  += MAX (child_requisition.width,
                              action_requisition.width);
  requisition->height += (child_requisition.height +
                          border_width +
                          action_requisition.height);
}

static void
gimp_tool_overlay_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GtkContainer    *container = GTK_CONTAINER (widget);
  GimpToolOverlay *overlay   = GIMP_TOOL_OVERLAY (widget);
  GtkWidget       *child     = gtk_bin_get_child (GTK_BIN (widget));
  GtkRequisition   action_requisition;
  GtkAllocation    child_allocation;
  GtkAllocation    action_allocation;
  gint             border_width;

  gtk_widget_set_allocation (widget, allocation);

  border_width = gtk_container_get_border_width (container);

  gtk_widget_size_request (overlay->action_area, &action_requisition);

  if (child && gtk_widget_get_visible (child))
    {
      child_allocation.x      = allocation->x + border_width;
      child_allocation.y      = allocation->y + border_width;
      child_allocation.width  = MAX (allocation->width  - 2 * border_width, 0);
      child_allocation.height = MAX (allocation->height -
                                     3 * border_width -
                                     action_requisition.height, 0);

      gtk_widget_size_allocate (child, &child_allocation);
    }

  action_allocation.x = allocation->x + border_width;
  action_allocation.y = (child_allocation.y + child_allocation.height +
                         border_width);
  action_allocation.width  = MAX (allocation->width  - 2 * border_width, 0);
  action_allocation.height = MAX (action_requisition.height, 0);

  gtk_widget_size_allocate (overlay->action_area, &action_allocation);
}

static gboolean
gimp_tool_overlay_expose (GtkWidget      *widget,
                          GdkEventExpose *eevent)
{
  cairo_t       *cr = gdk_cairo_create (gtk_widget_get_window (widget));
  GtkStyle      *style;
  GtkAllocation  allocation;
  gint           border_width;
  gint           inner_width;
  gint           inner_height;

  style = gtk_widget_get_style (widget);
  gtk_widget_get_allocation (widget, &allocation);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  inner_width  = allocation.width  - border_width / 2;
  inner_height = allocation.height - border_width / 2;

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, eevent->region);
  cairo_clip_preserve (cr);
  cairo_fill (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);

#define TO_RAD(deg) (deg * (G_PI / 180.0))

  cairo_arc (cr,
             border_width,
             border_width,
             border_width,
             TO_RAD (180),
             TO_RAD (270));
  cairo_line_to (cr,
                 allocation.width - border_width,
                 0);

  cairo_arc (cr,
             allocation.width - border_width,
             border_width,
             border_width,
             TO_RAD (270),
             TO_RAD (0));
  cairo_line_to (cr,
                 allocation.width,
                 allocation.height - border_width);

  cairo_arc (cr,
             allocation.width  - border_width,
             allocation.height - border_width,
             border_width,
             TO_RAD (0),
             TO_RAD (90));
  cairo_line_to (cr,
                 border_width,
                 allocation.height);

  cairo_arc (cr,
             border_width,
             allocation.height - border_width,
             border_width,
             TO_RAD (90),
             TO_RAD (180));
  cairo_close_path (cr);

  cairo_fill (cr);

  cairo_destroy (cr);

  return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, eevent);
}

static void
gimp_tool_overlay_forall (GtkContainer *container,
                          gboolean      include_internals,
                          GtkCallback   callback,
                          gpointer      callback_data)
{
  GTK_CONTAINER_CLASS (parent_class)->forall (container, include_internals,
                                              callback, callback_data);

  if (include_internals)
    {
      GimpToolOverlay *overlay = GIMP_TOOL_OVERLAY (container);

      if (overlay->action_area)
        (* callback) (overlay->action_area, callback_data);
    }
}

GtkWidget *
gimp_tool_overlay_new (GimpToolInfo *tool_info,
                       const gchar  *desc,
                       ...)
{
  GtkWidget   *overlay;
  const gchar *stock_id;
  va_list      args;

  g_return_val_if_fail (GIMP_IS_TOOL_INFO (tool_info), NULL);

  stock_id = gimp_viewable_get_stock_id (GIMP_VIEWABLE (tool_info));

  overlay = g_object_new (GIMP_TYPE_TOOL_OVERLAY, NULL);

  va_start (args, desc);
  gimp_tool_overlay_add_buttons_valist (GIMP_TOOL_OVERLAY (overlay), args);
  va_end (args);

  return overlay;
}

void
gimp_tool_overlay_response (GimpToolOverlay *overlay,
                            gint             response_id)
{
  g_return_if_fail (GIMP_IS_TOOL_OVERLAY (overlay));

  g_signal_emit (overlay, signals[RESPONSE], 0,
		 response_id);
}

void
gimp_tool_overlay_add_buttons_valist (GimpToolOverlay *overlay,
                                      va_list          args)
{
  const gchar *button_text;
  gint         response_id;

  g_return_if_fail (GIMP_IS_TOOL_OVERLAY (overlay));

  while ((button_text = va_arg (args, const gchar *)))
    {
      response_id = va_arg (args, gint);

      gimp_tool_overlay_add_button (overlay, button_text, response_id);
    }
}

static void
action_widget_activated (GtkWidget       *widget,
                         GimpToolOverlay *overlay)
{
  ResponseData *ad = get_response_data (widget, FALSE);

  gimp_tool_overlay_response (overlay, ad->response_id);
}

GtkWidget *
gimp_tool_overlay_add_button (GimpToolOverlay *overlay,
                              const gchar     *button_text,
                              gint             response_id)
{
  GtkWidget    *button;
  ResponseData *ad;
  guint         signal_id;
  GClosure     *closure;

  g_return_val_if_fail (GIMP_IS_TOOL_OVERLAY (overlay), NULL);
  g_return_val_if_fail (button_text != NULL, NULL);

  button = gtk_button_new_from_stock (button_text);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show (button);

  ad = get_response_data (button, TRUE);

  ad->response_id = response_id;

  signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);

  closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                   G_OBJECT (overlay));
  g_signal_connect_closure_by_id (button, signal_id, 0,
                                  closure, FALSE);

  gtk_box_pack_end (GTK_BOX (overlay->action_area), button, FALSE, TRUE, 0);

  if (response_id == GTK_RESPONSE_HELP)
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (overlay->action_area),
                                        button, TRUE);

  return button;
}

static void
response_data_free (gpointer data)
{
  g_slice_free (ResponseData, data);
}

static ResponseData *
get_response_data (GtkWidget *widget,
		   gboolean   create)
{
  ResponseData *ad = g_object_get_data (G_OBJECT (widget),
                                        "gimp-tool-overlay-response-data");

  if (! ad && create)
    {
      ad = g_slice_new (ResponseData);

      g_object_set_data_full (G_OBJECT (widget),
                              "gimp-tool-overlay-response-data",
                              ad, response_data_free);
    }

  return ad;
}
