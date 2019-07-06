/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdockwindow.h
 * Copyright (C) 2001-2005 Michael Natterer <mitch@gimp.org>
 * Copyright (C)      2009 Martin Nordholts <martinn@src.gnome.org>
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

#ifndef __GIMP_DOCK_WINDOW_H__
#define __GIMP_DOCK_WINDOW_H__


#include "widgets/gimpwindow.h"


#define GIMP_TYPE_DOCK_WINDOW            (gimp_dock_window_get_type ())
#define GIMP_DOCK_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_DOCK_WINDOW, GimpDockWindow))
#define GIMP_DOCK_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_DOCK_WINDOW, GimpDockWindowClass))
#define GIMP_IS_DOCK_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_DOCK_WINDOW))
#define GIMP_IS_DOCK_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_DOCK_WINDOW))
#define GIMP_DOCK_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_DOCK_WINDOW, GimpDockWindowClass))


typedef struct _GimpDockWindowClass    GimpDockWindowClass;
typedef struct _GimpDockWindowPrivate  GimpDockWindowPrivate;

/**
 * GimpDockWindow:
 *
 * A top-level window containing GimpDocks.
 */
struct _GimpDockWindow
{
  GimpWindow  parent_instance;

  GimpDockWindowPrivate *p;
};

struct _GimpDockWindowClass
{
  GimpWindowClass  parent_class;
};


GType               gimp_dock_window_get_type         (void) G_GNUC_CONST;
gint                gimp_dock_window_get_id           (GimpDockWindow *dock_window);
void                gimp_dock_window_add_dock         (GimpDockWindow *dock_window,
                                                       GimpDock       *dock,
                                                       gint            index);
void                gimp_dock_window_remove_dock      (GimpDockWindow *dock_window,
                                                       GimpDock       *dock);
GimpUIManager     * gimp_dock_window_get_ui_manager   (GimpDockWindow *dock_window);
GimpDockWindow    * gimp_dock_window_from_dock        (GimpDock       *dock);
GList             * gimp_dock_window_get_docks        (GimpDockWindow *dock_window);
GimpDock          * gimp_dock_window_get_dock         (GimpDockWindow *dock_window);


#endif /* __GIMP_DOCK_WINDOW_H__ */
