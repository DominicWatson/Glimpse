/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdockcolumns.c
 * Copyright (C) 2009 Martin Nordholts <martinn@src.gnome.org>
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

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpmarshal.h"

#include "dialogs/dialogs.h"

#include "gimpdialogfactory.h"
#include "gimpdock.h"
#include "gimpdockable.h"
#include "gimpdockbook.h"
#include "gimpdockcolumns.h"
#include "gimpmenudock.h"
#include "gimppanedbox.h"

#include "gimp-log.h"

enum
{
  DOCK_ADDED,
  DOCK_REMOVED,
  LAST_SIGNAL
};


struct _GimpDockColumnsPrivate
{
  GList     *docks;

  GtkWidget *paned_hbox;
};


static gboolean  gimp_dock_columns_dropped_cb        (GtkWidget         *source,
                                                      gint               insert_index,
                                                      gpointer           data);
static void      gimp_dock_columns_real_dock_added   (GimpDockColumns   *dock_columns,
                                                      GimpDock          *dock);
static void      gimp_dock_columns_real_dock_removed (GimpDockColumns   *dock_columns,
                                                      GimpDock          *dock);
static void      gimp_dock_columns_dock_book_removed (GimpDockColumns   *dock_columns,
                                                      GimpDockbook      *dockbook,
                                                      GimpDock          *dock);


G_DEFINE_TYPE (GimpDockColumns, gimp_dock_columns, GTK_TYPE_HBOX)

#define parent_class gimp_dock_columns_parent_class

static guint dock_columns_signals[LAST_SIGNAL] = { 0 };


static void
gimp_dock_columns_class_init (GimpDockColumnsClass *klass)
{
  klass->dock_added   = gimp_dock_columns_real_dock_added;
  klass->dock_removed = gimp_dock_columns_real_dock_removed;

  dock_columns_signals[DOCK_ADDED] =
    g_signal_new ("dock-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDockColumnsClass, dock_added),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_DOCK);

  dock_columns_signals[DOCK_REMOVED] =
    g_signal_new ("dock-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDockColumnsClass, dock_removed),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_DOCK);

  g_type_class_add_private (klass, sizeof (GimpDockColumnsPrivate));
}

static void
gimp_dock_columns_init (GimpDockColumns *dock_columns)
{
  dock_columns->p = G_TYPE_INSTANCE_GET_PRIVATE (dock_columns,
                                                 GIMP_TYPE_DOCK_COLUMNS,
                                                 GimpDockColumnsPrivate);

  dock_columns->p->paned_hbox = gimp_paned_box_new (FALSE, 0,
                                                    GTK_ORIENTATION_HORIZONTAL);
  gimp_paned_box_set_dropped_cb (GIMP_PANED_BOX (dock_columns->p->paned_hbox),
                                 gimp_dock_columns_dropped_cb,
                                 dock_columns);
  gtk_container_add (GTK_CONTAINER (dock_columns), dock_columns->p->paned_hbox);
  gtk_widget_show (dock_columns->p->paned_hbox);
}

static gboolean
gimp_dock_columns_dropped_cb (GtkWidget         *source,
                              gint               insert_index,
                              gpointer           data)
{
  GimpDockColumns *dock_columns = GIMP_DOCK_COLUMNS (data);
  GimpDockable    *dockable     = gimp_dockbook_drag_source_to_dockable (source);
  GtkWidget       *dock         = NULL;
  GtkWidget       *dockbook     = NULL;

  if (!dockable )
    return FALSE;

  /* Create and insert new dock into columns */
  dock = gimp_menu_dock_new ();
  gimp_dock_columns_add_dock (dock_columns, GIMP_DOCK (dock), insert_index);

  /* Put a now dockbook in the dock */
  dockbook = gimp_dockbook_new (gimp_dock_get_dialog_factory (GIMP_DOCK (dock))->menu_factory);
  g_object_ref (dockbook);
  gimp_dock_add_book (GIMP_DOCK (dock), GIMP_DOCKBOOK (dockbook), -1);

  /* Move the dockable to the new dockbook */
  g_object_ref (dockable);
  gimp_dockbook_remove (dockable->dockbook, dockable);
  gimp_dockbook_add (GIMP_DOCKBOOK (dockbook), dockable, -1);
  g_object_unref (dockable);
  g_object_unref (dockbook);

  /* Show! */
  gtk_widget_show (GTK_WIDGET (dock));

  return TRUE;
}

static void
gimp_dock_columns_real_dock_added (GimpDockColumns *dock_columns,
                                   GimpDock        *dock)
{
}

static void
gimp_dock_columns_real_dock_removed (GimpDockColumns *dock_columns,
                                     GimpDock        *dock)
{
}

static void
gimp_dock_columns_dock_book_removed (GimpDockColumns *dock_columns,
                                     GimpDockbook    *dockbook,
                                     GimpDock        *dock)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));

  if (gimp_dock_get_dockbooks (dock) == NULL)
    gimp_dock_columns_remove_dock (dock_columns, dock);
}


/**
 * gimp_dock_columns_add_dock:
 * @dock_columns:
 * @dock:
 *
 * Add a dock, added to a horizontal GimpPanedBox.
 **/
void
gimp_dock_columns_add_dock (GimpDockColumns *dock_columns,
                            GimpDock        *dock,
                            gint             index)
{
  g_return_if_fail (GIMP_IS_DOCK_COLUMNS (dock_columns));
  g_return_if_fail (GIMP_IS_DOCK (dock));

  GIMP_LOG (DND, "Adding GimpDock %p to GimpDockColumns %p", dock, dock_columns);

  dock_columns->p->docks = g_list_append (dock_columns->p->docks, dock);

  gimp_paned_box_add_widget (GIMP_PANED_BOX (dock_columns->p->paned_hbox),
                             GTK_WIDGET (dock),
                             index);

  g_signal_connect_object (dock, "book-removed",
                           G_CALLBACK (gimp_dock_columns_dock_book_removed),
                           dock_columns,
                           G_CONNECT_SWAPPED);

  g_signal_emit (dock_columns, dock_columns_signals[DOCK_ADDED], 0, dock);
}


void
gimp_dock_columns_remove_dock (GimpDockColumns *dock_columns,
                               GimpDock        *dock)
{
  g_return_if_fail (GIMP_IS_DOCK_COLUMNS (dock_columns));
  g_return_if_fail (GIMP_IS_DOCK (dock));

  GIMP_LOG (DND, "Removing GimpDock %p from GimpDockColumns %p", dock, dock_columns);

  dock_columns->p->docks = g_list_remove (dock_columns->p->docks, dock);

  g_signal_handlers_disconnect_by_func (dock,
                                        gimp_dock_columns_dock_book_removed,
                                        dock_columns);

  g_object_ref (dock);
  gimp_paned_box_remove_widget (GIMP_PANED_BOX (dock_columns->p->paned_hbox),
                                GTK_WIDGET (dock));

  g_signal_emit (dock_columns, dock_columns_signals[DOCK_REMOVED], 0, dock);
  g_object_unref (dock);
}

GList *
gimp_dock_columns_get_docks (GimpDockColumns *dock_columns)
{
  return dock_columns->p->docks;
}
