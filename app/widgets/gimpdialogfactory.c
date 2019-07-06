/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdialogfactory.c
 * Copyright (C) 2001-2008 Michael Natterer <mitch@gimp.org>
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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpconfig/gimpconfig.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

/* FIXME: Remove when the toolbox and dock window factories have been
 * merged
 */
#include "dialogs/dialogs.h"

#include "core/gimpcontext.h"
#include "core/gimpmarshal.h"

#include "gimpcursor.h"
#include "gimpdialogfactory.h"
#include "gimpdock.h"
#include "gimpdockbook.h"
#include "gimpdockable.h"
#include "gimpdockwindow.h"
#include "gimpmenufactory.h"
#include "gimpsessioninfo.h"

#include "gimp-log.h"


typedef enum
{
  GIMP_DIALOGS_SHOWN,
  GIMP_DIALOGS_HIDDEN_EXPLICITLY,  /* user used the Tab key to hide dialogs */
  GIMP_DIALOGS_HIDDEN_WITH_DISPLAY /* dialogs are hidden with the display   */
} GimpDialogsState;

enum
{
  DOCK_WINDOW_ADDED,
  DOCK_WINDOW_REMOVED,
  LAST_SIGNAL
};


struct _GimpDialogFactoryPrivate
{
  GimpContext           *context;
  GimpMenuFactory       *menu_factory;

  GList                 *open_dialogs;
  GList                 *session_infos;

  GList                 *registered_dialogs;

  gboolean               toggle_visibility;
};


static void        gimp_dialog_factory_dispose              (GObject                *object);
static void        gimp_dialog_factory_finalize             (GObject                *object);
static GtkWidget * gimp_dialog_factory_constructor          (GimpDialogFactory      *factory,
                                                             GimpDialogFactoryEntry *entry,
                                                             GimpContext            *context,
                                                             GimpUIManager          *ui_manager,
                                                             gint                    view_size);
static void        gimp_dialog_factory_set_widget_data      (GtkWidget              *dialog,
                                                             GimpDialogFactory      *factory,
                                                             GimpDialogFactoryEntry *entry);
static void        gimp_dialog_factory_unset_widget_data    (GtkWidget              *dialog);
static gboolean    gimp_dialog_factory_set_user_pos         (GtkWidget              *dialog,
                                                             GdkEventConfigure      *cevent,
                                                             gpointer                data);
static gboolean    gimp_dialog_factory_dialog_configure     (GtkWidget              *dialog,
                                                             GdkEventConfigure      *cevent,
                                                             GimpDialogFactory      *factory);
static void        gimp_dialog_factories_save_foreach       (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             GimpConfigWriter       *writer);
static void        gimp_dialog_factories_restore_foreach    (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             gpointer                data);
static void        gimp_dialog_factories_clear_foreach      (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             gpointer                data);
static void        gimp_dialog_factories_hide_foreach       (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             gpointer                data);
static void        gimp_dialog_factories_show_foreach       (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             gpointer                data);
static void        gimp_dialog_factories_set_busy_foreach   (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             gpointer                data);
static void        gimp_dialog_factories_unset_busy_foreach (gconstpointer           key,
                                                             GimpDialogFactory      *factory,
                                                             gpointer                data);


G_DEFINE_TYPE (GimpDialogFactory, gimp_dialog_factory, GIMP_TYPE_OBJECT)

#define parent_class gimp_dialog_factory_parent_class

static guint factory_signals[LAST_SIGNAL] = { 0 };

static GimpDialogsState dialogs_state = GIMP_DIALOGS_SHOWN;


static void
gimp_dialog_factory_class_init (GimpDialogFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose  = gimp_dialog_factory_dispose;
  object_class->finalize = gimp_dialog_factory_finalize;

  klass->factories = g_hash_table_new (g_str_hash, g_str_equal);

  factory_signals[DOCK_WINDOW_ADDED] =
    g_signal_new ("dock-window-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GimpDialogFactoryClass, dock_window_added),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_DOCK_WINDOW);

  factory_signals[DOCK_WINDOW_REMOVED] =
    g_signal_new ("dock-window-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GimpDialogFactoryClass, dock_window_removed),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_DOCK_WINDOW);

  g_type_class_add_private (klass, sizeof (GimpDialogFactoryPrivate));
}

static void
gimp_dialog_factory_init (GimpDialogFactory *factory)
{
  factory->p = G_TYPE_INSTANCE_GET_PRIVATE (factory,
                                            GIMP_TYPE_DIALOG_FACTORY,
                                            GimpDialogFactoryPrivate);
}

static void
gimp_dialog_factory_dispose (GObject *object)
{
  GimpDialogFactory *factory = GIMP_DIALOG_FACTORY (object);
  GList             *list;
  gpointer           key;

  /*  start iterating from the beginning each time we destroyed a
   *  toplevel because destroying a dock may cause lots of items
   *  to be removed from factory->p->open_dialogs
   */
  while (factory->p->open_dialogs)
    {
      for (list = factory->p->open_dialogs; list; list = g_list_next (list))
        {
          if (gtk_widget_is_toplevel (list->data))
            {
              gtk_widget_destroy (GTK_WIDGET (list->data));
              break;
            }
        }

      /*  the list being non-empty without any toplevel is an error,
       *  so eek and chain up
       */
      if (! list)
        {
          g_warning ("%s: stale non-toplevel entries in factory->p->open_dialogs",
                     G_STRFUNC);
          break;
        }
    }

  if (factory->p->open_dialogs)
    {
      g_list_free (factory->p->open_dialogs);
      factory->p->open_dialogs = NULL;
    }

  if (factory->p->session_infos)
    {
      g_list_foreach (factory->p->session_infos, (GFunc) g_object_unref, NULL);
      g_list_free (factory->p->session_infos);
      factory->p->session_infos = NULL;
    }

  if (strcmp (gimp_object_get_name (factory), "toolbox") == 0)
    key = "";
  else
    key = (gpointer)gimp_object_get_name (factory);

  g_hash_table_remove (GIMP_DIALOG_FACTORY_GET_CLASS (object)->factories,
                       key);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_dialog_factory_finalize (GObject *object)
{
  GimpDialogFactory *factory = GIMP_DIALOG_FACTORY (object);
  GList             *list;

  for (list = factory->p->registered_dialogs; list; list = g_list_next (list))
    {
      GimpDialogFactoryEntry *entry = list->data;

      g_free (entry->identifier);
      g_free (entry->name);
      g_free (entry->blurb);
      g_free (entry->stock_id);
      g_free (entry->help_id);

      g_slice_free (GimpDialogFactoryEntry, entry);
    }

  if (factory->p->registered_dialogs)
    {
      g_list_free (factory->p->registered_dialogs);
      factory->p->registered_dialogs = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GimpDialogFactory *
gimp_dialog_factory_new (const gchar           *name,
                         GimpContext           *context,
                         GimpMenuFactory       *menu_factory,
                         gboolean               toggle_visibility)
{
  GimpDialogFactory *factory;
  gpointer           key;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (! menu_factory || GIMP_IS_MENU_FACTORY (menu_factory),
                        NULL);

  if (gimp_dialog_factory_from_name (name))
    {
      g_warning ("%s: dialog factory \"%s\" already exists",
                 G_STRFUNC, name);
      return NULL;
    }

  factory = g_object_new (GIMP_TYPE_DIALOG_FACTORY, NULL);

  gimp_object_set_name (GIMP_OBJECT (factory), name);

  /*  hack to keep the toolbox on the pool position  */
  if (strcmp (name, "toolbox") == 0)
    key = "";
  else
    key = (gpointer)gimp_object_get_name (factory);

  g_hash_table_insert (GIMP_DIALOG_FACTORY_GET_CLASS (factory)->factories,
                       key, factory);

  factory->p->context           = context;
  factory->p->menu_factory      = menu_factory;
  factory->p->toggle_visibility = toggle_visibility;

  return factory;
}

GimpDialogFactory *
gimp_dialog_factory_from_name (const gchar *name)
{
  GimpDialogFactoryClass *factory_class;
  GimpDialogFactory      *factory;

  g_return_val_if_fail (name != NULL, NULL);

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);
  if (! factory_class)
    return NULL;

  /*  hack to keep the toolbox on the pool position  */
  if (strcmp (name, "toolbox") == 0)
    name = "";

  factory =
    (GimpDialogFactory *) g_hash_table_lookup (factory_class->factories, name);

  return factory;
}

void
gimp_dialog_factory_register_entry (GimpDialogFactory *factory,
                                    const gchar       *identifier,
                                    const gchar       *name,
                                    const gchar       *blurb,
                                    const gchar       *stock_id,
                                    const gchar       *help_id,
                                    GimpDialogNewFunc  new_func,
                                    gint               view_size,
                                    gboolean           singleton,
                                    gboolean           session_managed,
                                    gboolean           remember_size,
                                    gboolean           remember_if_open,
                                    gboolean           dockable)
{
  GimpDialogFactoryEntry *entry;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (identifier != NULL);

  entry = g_slice_new0 (GimpDialogFactoryEntry);

  entry->identifier       = g_strdup (identifier);
  entry->name             = g_strdup (name);
  entry->blurb            = g_strdup (blurb);
  entry->stock_id         = g_strdup (stock_id);
  entry->help_id          = g_strdup (help_id);
  entry->new_func         = new_func;
  entry->view_size        = view_size;
  entry->singleton        = singleton ? TRUE : FALSE;
  entry->session_managed  = session_managed ? TRUE : FALSE;
  entry->remember_size    = remember_size ? TRUE : FALSE;
  entry->remember_if_open = remember_if_open ? TRUE : FALSE;
  entry->dockable         = dockable ? TRUE : FALSE;

  factory->p->registered_dialogs = g_list_prepend (factory->p->registered_dialogs,
                                                   entry);
}

GimpDialogFactoryEntry *
gimp_dialog_factory_find_entry (GimpDialogFactory *factory,
                                const gchar       *identifier)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  for (list = factory->p->registered_dialogs; list; list = g_list_next (list))
    {
      GimpDialogFactoryEntry *entry = list->data;

      if (! strcmp (identifier, entry->identifier))
        return entry;
    }

  return NULL;
}

GimpSessionInfo *
gimp_dialog_factory_find_session_info (GimpDialogFactory *factory,
                                       const gchar       *identifier)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  for (list = factory->p->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *info = list->data;

      if (gimp_session_info_get_factory_entry (info) &&
          g_str_equal (identifier,
                       gimp_session_info_get_factory_entry (info)->identifier))
        {
          return info;
        }
    }

  return NULL;
}

/**
 * gimp_dialog_factory_dialog_sane:
 * @factory:
 * @widget_factory:
 * @widget_entry:
 * @widget:
 *
 * Makes sure that the @widget with the given @widget_entry that was
 * created by the given @widget_factory belongs to @efactory.
 *
 * Returns: %TRUE if that is the case, %FALSE otherwise.
 **/
static gboolean
gimp_dialog_factory_dialog_sane (GimpDialogFactory      *factory,
                                 GimpDialogFactory      *widget_factory,
                                 GimpDialogFactoryEntry *widget_entry,
                                 GtkWidget              *widget)
{
  if (! widget_factory || ! widget_entry)
    {
      g_warning ("%s: dialog was not created by a GimpDialogFactory",
                 G_STRFUNC);
      return FALSE;
    }

  if (widget_factory != factory)
    {
      g_warning ("%s: dialog was created by a different GimpDialogFactory",
                 G_STRFUNC);
      return FALSE;
    }

  return TRUE;
}

static GtkWidget *
gimp_dialog_factory_dialog_new_internal (GimpDialogFactory *factory,
                                         GdkScreen         *screen,
                                         GimpContext       *context,
                                         GimpUIManager     *ui_manager,
                                         const gchar       *identifier,
                                         gint               view_size,
                                         gboolean           return_existing,
                                         gboolean           present)
{
  GimpDialogFactoryEntry *entry;
  GtkWidget              *dialog = NULL;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  entry = gimp_dialog_factory_find_entry (factory, identifier);

  if (! entry)
    {
      g_warning ("%s: no entry registered for \"%s\"",
                 G_STRFUNC, identifier);
      return NULL;
    }

  if (! entry->new_func)
    {
      g_warning ("%s: entry for \"%s\" has no constructor",
                 G_STRFUNC, identifier);
      return NULL;
    }

  /*  a singleton dialog is always returned if it already exisits  */
  if (return_existing || entry->singleton)
    {
      GimpSessionInfo *info;

      info = gimp_dialog_factory_find_session_info (factory, identifier);

      if (info)
        dialog = gimp_session_info_get_widget (info);
    }

  /*  create the dialog if it was not found  */
  if (! dialog)
    {
      GtkWidget *dock              = NULL;
      gboolean   called_from_raise = FALSE;

      called_from_raise = (context == NULL);
                                 
      if (entry->dockable && called_from_raise)
        {
          GtkWidget *dockbook;

          /*  It doesn't make sense to have a dockable without a dock
           *  so create one. Create a new dock _before_ creating the
           *  dialog. We do this because the new dockable needs to be
           *  created in its dock's context.
           */
          dock     = gimp_dialog_factory_dock_with_window_new (factory, screen);
          dockbook = gimp_dockbook_new (factory->p->menu_factory);

          gimp_dock_add_book (GIMP_DOCK (dock),
                              GIMP_DOCKBOOK (dockbook),
                              0);
        }

      /*  Create the new dialog in the appropriate context which is
       *  - the passed context if not NULL
       *  - the newly created dock's context if we just created it
       *  - the factory's context, which happens when raising a toplevel
       *    dialog was the original request.
       */
      if (view_size < GIMP_VIEW_SIZE_TINY)
        view_size = entry->view_size;

      if (context)
        dialog = gimp_dialog_factory_constructor (factory, entry,
                                                  context,
                                                  ui_manager,
                                                  view_size);
      else if (dock)
        dialog = gimp_dialog_factory_constructor (factory, entry,
                                                  gimp_dock_get_context (GIMP_DOCK (dock)),
                                                  gimp_dock_get_ui_manager (GIMP_DOCK (dock)),
                                                  view_size);
      else
        dialog = gimp_dialog_factory_constructor (factory, entry,
                                                  factory->p->context,
                                                  ui_manager,
                                                  view_size);

      if (dialog)
        {
          gimp_dialog_factory_set_widget_data (dialog, factory, entry);

          /*  If we created a dock before, the newly created dialog is
           *  supposed to be a GimpDockable.
           */
          if (dock)
            {
              if (GIMP_IS_DOCKABLE (dialog))
                {
                  gimp_dock_add (GIMP_DOCK (dock), GIMP_DOCKABLE (dialog),
                                 0, 0);

                  gtk_widget_show (dock);
                }
              else
                {
                  g_warning ("%s: GimpDialogFactory is a dockable factory "
                             "but constructor for \"%s\" did not return a "
                             "GimpDockable",
                             G_STRFUNC, identifier);

                  gtk_widget_destroy (dialog);
                  gtk_widget_destroy (dock);

                  dialog = NULL;
                  dock   = NULL;
                }
            }
        }
      else if (dock)
        {
          g_warning ("%s: constructor for \"%s\" returned NULL",
                     G_STRFUNC, identifier);

          gtk_widget_destroy (dock);

          dock = NULL;
        }

      if (dialog)
        gimp_dialog_factory_add_dialog (factory, dialog);
    }

  /*  Finally, if we found an existing dialog or created a new one, raise it.
   */
  if (! dialog)
    return NULL;

  if (gtk_widget_is_toplevel (dialog))
    {
      gtk_window_set_screen (GTK_WINDOW (dialog), screen);

      if (present)
        gtk_window_present (GTK_WINDOW (dialog));
    }
  else if (GIMP_IS_DOCKABLE (dialog))
    {
      GimpDockable *dockable = GIMP_DOCKABLE (dialog);

      if (gimp_dockable_get_dockbook (dockable) &&
          gimp_dockbook_get_dock (gimp_dockable_get_dockbook (dockable)))
        {
          GtkNotebook *notebook = GTK_NOTEBOOK (gimp_dockable_get_dockbook (dockable));
          gint         num      = gtk_notebook_page_num (notebook, dialog);

          if (num != -1)
            {
              gtk_notebook_set_current_page (notebook, num);

              gimp_dockable_blink (dockable);
            }
        }

      if (present)
        {
          GtkWidget *toplevel = gtk_widget_get_toplevel (dialog);

          if (GTK_IS_WINDOW (toplevel))
            gtk_window_present (GTK_WINDOW (toplevel));
        }
    }

  return dialog;
}

/**
 * gimp_dialog_factory_dialog_new:
 * @factory:      a #GimpDialogFactory
 * @screen:       the #GdkScreen the dialog should appear on
 * @identifier:   the identifier of the dialog as registered with
 *                gimp_dialog_factory_register_entry()
 * @view_size:    the initial preview size
 * @present:      whether gtk_window_present() should be called
 *
 * Creates a new toplevel dialog or a #GimpDockable, depending on whether
 * %factory is a toplevel of dockable factory.
 *
 * Return value: the newly created dialog or an already existing singleton
 *               dialog.
 **/
GtkWidget *
gimp_dialog_factory_dialog_new (GimpDialogFactory *factory,
                                GdkScreen         *screen,
                                const gchar       *identifier,
                                gint               view_size,
                                gboolean           present)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  return gimp_dialog_factory_dialog_new_internal (factory,
                                                  screen,
                                                  factory->p->context,
                                                  NULL,
                                                  identifier,
                                                  view_size,
                                                  FALSE,
                                                  present);
}

GimpContext *
gimp_dialog_factory_get_context (GimpDialogFactory *factory)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);

  return factory->p->context;
}

GimpMenuFactory *
gimp_dialog_factory_get_menu_factory (GimpDialogFactory *factory)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);

  return factory->p->menu_factory;
}

GList *
gimp_dialog_factory_get_open_dialogs (GimpDialogFactory *factory)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);

  return factory->p->open_dialogs;
}

GList *
gimp_dialog_factory_get_session_infos (GimpDialogFactory *factory)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);

  return factory->p->session_infos;
}

void
gimp_dialog_factory_add_session_info (GimpDialogFactory *factory,
                                      GimpSessionInfo   *info)
{
  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));

  /* We want to append rather than prepend so that the serialized
   * order in sessionrc remains the same
   */
  factory->p->session_infos = g_list_append (factory->p->session_infos, info);
}

/**
 * gimp_dialog_factory_dialog_raise:
 * @factory:      a #GimpDialogFactory
 * @screen:       the #GdkScreen the dialog should appear on
 * @identifiers:  a '|' separated list of identifiers of dialogs as
 *                registered with gimp_dialog_factory_register_entry()
 * @view_size:    the initial preview size if a dialog needs to be created
 *
 * Raises any of a list of already existing toplevel dialog or
 * #GimpDockable if it was already created by this %facory.
 *
 * Implicitly creates the first dialog in the list if none of the dialogs
 * were found.
 *
 * Return value: the raised or newly created dialog.
 **/
GtkWidget *
gimp_dialog_factory_dialog_raise (GimpDialogFactory *factory,
                                  GdkScreen         *screen,
                                  const gchar       *identifiers,
                                  gint               view_size)
{
  GtkWidget *dialog;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (identifiers != NULL, NULL);

  /*  If the identifier is a list, try to find a matching dialog and
   *  raise it. If there's no match, use the first list item.
   */
  if (strchr (identifiers, '|'))
    {
      gchar **ids = g_strsplit (identifiers, "|", 0);
      gint    i;

      for (i = 0; ids[i]; i++)
        {
          GimpSessionInfo *info;

          info = gimp_dialog_factory_find_session_info (factory, ids[i]);
          if (info && gimp_session_info_get_widget (info))
            break;
        }

      dialog = gimp_dialog_factory_dialog_new_internal (factory,
                                                        screen,
                                                        NULL,
                                                        NULL,
                                                        ids[i] ? ids[i] : ids[0],
                                                        view_size,
                                                        TRUE,
                                                        TRUE);
      g_strfreev (ids);
    }
  else
    {
      dialog = gimp_dialog_factory_dialog_new_internal (factory,
                                                        screen,
                                                        NULL,
                                                        NULL,
                                                        identifiers,
                                                        view_size,
                                                        TRUE,
                                                        TRUE);
    }

  return dialog;
}

/**
 * gimp_dialog_factory_dockable_new:
 * @factory:      a #GimpDialogFactory
 * @dock:         a #GimpDock crated by this %factory.
 * @identifier:   the identifier of the dialog as registered with
 *                gimp_dialog_factory_register_entry()
 * @view_size:
 *
 * Creates a new #GimpDockable in the context of the #GimpDock it will be
 * added to.
 *
 * Implicitly raises & returns an already existing singleton dockable,
 * so callers should check that gimp_dockable_get_dockbook (dockable)
 * is NULL before trying to add it to it's #GimpDockbook.
 *
 * Return value: the newly created #GimpDockable or an already existing
 *               singleton dockable.
 **/
GtkWidget *
gimp_dialog_factory_dockable_new (GimpDialogFactory *factory,
                                  GimpDock          *dock,
                                  const gchar       *identifier,
                                  gint               view_size)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  return gimp_dialog_factory_dialog_new_internal (factory,
                                                  gtk_widget_get_screen (GTK_WIDGET (dock)),
                                                  gimp_dock_get_context (dock),
                                                  gimp_dock_get_ui_manager (dock),
                                                  identifier,
                                                  view_size,
                                                  FALSE,
                                                  FALSE);
}

/**
 * gimp_dialog_factory_dock_with_window_new:
 * @factory: a #GimpDialogFacotry
 * @screen:  the #GdkScreen the dock window should appear on
 *
 * Returns a new #GimpDock in this %factory's context, put inside a
 * #GimpDockWindow. We use a function pointer passed to this
 * %factory's constructor instead of simply gimp_dock_new() because we
 * may want different instances of #GimpDialogFactory create different
 * subclasses of #GimpDock.
 *
 * Return value: the newly created #GimpDock.
 **/
GtkWidget *
gimp_dialog_factory_dock_with_window_new (GimpDialogFactory *factory,
                                          GdkScreen         *screen)
{
  GtkWidget     *dock_window = NULL;
  gchar         *dock_id     = NULL;
  GtkWidget     *dock        = NULL;
  GimpUIManager *ui_manager  = NULL;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  /* Create a dock window to put the dock in. We need to create the
   * dock window before the dock because the dock has a dependency to
   * the ui manager in the dock window
   */
  dock_window =
    gimp_dialog_factory_dialog_new (factory,
                                    screen,
                                    (strcmp ("toolbox", gimp_object_get_name (factory)) == 0 ?
                                     "gimp-toolbox-window" :
                                     "gimp-dock-window"),
                                    -1 /*view_size*/,
                                    FALSE /*present*/);

  /* Create the dock */
  ui_manager = gimp_dock_window_get_ui_manager (GIMP_DOCK_WINDOW (dock_window));
  
  dock_id = g_strconcat ("gimp-", gimp_object_get_name (factory), NULL);
  dock    = gimp_dialog_factory_dialog_new (factory,
                                            screen,
                                            (strcmp ("toolbox", gimp_object_get_name (factory)) == 0 ?
                                             "gimp-toolbox" :
                                             "gimp-dock"),
                                            -1 /*view_size*/,
                                            FALSE /*present*/);
  g_free (dock_id);

  if (dock)
    {
      /* Put the dock in the dock window */
      gimp_dock_window_add_dock (GIMP_DOCK_WINDOW (dock_window),
                                 GIMP_DOCK (dock),
                                 -1);
    }

  return dock;
}

void
gimp_dialog_factory_add_dialog (GimpDialogFactory *factory,
                                GtkWidget         *dialog)
{
  GimpDialogFactory      *dialog_factory = NULL;
  GimpDialogFactoryEntry *entry          = NULL;
  GimpSessionInfo        *info           = NULL;
  GList                  *list           = NULL;
  gboolean                toplevel       = FALSE;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (GTK_IS_WIDGET (dialog));

  if (g_list_find (factory->p->open_dialogs, dialog))
    {
      g_warning ("%s: dialog already registered", G_STRFUNC);
      return;
    }

  dialog_factory = gimp_dialog_factory_from_widget (dialog, &entry);

  if (! gimp_dialog_factory_dialog_sane (factory,
                                         dialog_factory,
                                         entry,
                                         dialog))
    return;

  toplevel = gtk_widget_is_toplevel (dialog);

  if (entry) /* dialog is a toplevel (but not a GimpDockWindow) or a GimpDockable */
    {
      GIMP_LOG (DIALOG_FACTORY, "adding %s \"%s\"",
                toplevel ? "toplevel" : "dockable",
                entry->identifier);

      for (list = factory->p->session_infos; list; list = g_list_next (list))
        {
          GimpSessionInfo *current_info = list->data;

          if (gimp_session_info_get_factory_entry (current_info) == entry)
            {
              if (gimp_session_info_get_widget (current_info))
                {
                  if (gimp_session_info_is_singleton (current_info))
                    {
                      g_warning ("%s: singleton dialog \"%s\" created twice",
                                 G_STRFUNC, entry->identifier);

                      GIMP_LOG (DIALOG_FACTORY,
                                "corrupt session info: %p (widget %p)",
                                current_info,
                                gimp_session_info_get_widget (current_info));

                      return;
                    }

                  continue;
                }

              gimp_session_info_set_widget (current_info, dialog);

              GIMP_LOG (DIALOG_FACTORY,
                        "updating session info %p (widget %p) for %s \"%s\"",
                        current_info,  gimp_session_info_get_widget (current_info),
                        toplevel ? "toplevel" : "dockable",
                        entry->identifier);

              if (toplevel &&
                  gimp_session_info_is_session_managed (current_info) &&
                  ! gtk_widget_get_visible (dialog))
                {
                  gimp_session_info_apply_geometry (current_info);
                }

              info = current_info;
              break;
            }
        }

      if (! info)
        {
          info = gimp_session_info_new ();

          gimp_session_info_set_widget (info, dialog);

          GIMP_LOG (DIALOG_FACTORY,
                    "creating session info %p (widget %p) for %s \"%s\"",
                    info, gimp_session_info_get_widget (info),
                    toplevel ? "toplevel" : "dockable",
                    entry->identifier);

          gimp_session_info_set_factory_entry (info, entry);

          /*  if we create a new session info, we never call
           *  gimp_session_info_apply_geometry(), but still the
           *  dialog needs GDK_HINT_USER_POS so it keeps its
           *  position when hidden/shown within this(!) session.
           */
          if (gimp_session_info_is_session_managed (info))
            g_signal_connect (dialog, "configure-event",
                              G_CALLBACK (gimp_dialog_factory_set_user_pos),
                              NULL);

          factory->p->session_infos = g_list_append (factory->p->session_infos, info);
        }
    }

  /* Some special logic for dock windows */
  if (GIMP_IS_DOCK_WINDOW (dialog))
    {
      /*  let new docks appear at the pointer position  */
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

      g_signal_emit (factory, factory_signals[DOCK_WINDOW_ADDED], 0, dialog);
    }

  factory->p->open_dialogs = g_list_prepend (factory->p->open_dialogs, dialog);

  g_signal_connect_object (dialog, "destroy",
                           G_CALLBACK (gimp_dialog_factory_remove_dialog),
                           factory,
                           G_CONNECT_SWAPPED);

  if (gimp_session_info_is_session_managed (info))
    g_signal_connect_object (dialog, "configure-event",
                             G_CALLBACK (gimp_dialog_factory_dialog_configure),
                             factory,
                             0);
}

void
gimp_dialog_factory_add_foreign (GimpDialogFactory *factory,
                                 const gchar       *identifier,
                                 GtkWidget         *dialog)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (identifier != NULL);
  g_return_if_fail (GTK_IS_WIDGET (dialog));
  g_return_if_fail (gtk_widget_is_toplevel (dialog));

  dialog_factory = gimp_dialog_factory_from_widget (dialog, &entry);

  if (dialog_factory || entry)
    {
      g_warning ("%s: dialog was created by a GimpDialogFactory",
                 G_STRFUNC);
      return;
    }

  entry = gimp_dialog_factory_find_entry (factory, identifier);

  if (! entry)
    {
      g_warning ("%s: no entry registered for \"%s\"",
                 G_STRFUNC, identifier);
      return;
    }

  if (entry->new_func)
    {
      g_warning ("%s: entry for \"%s\" has a constructor (is not foreign)",
                 G_STRFUNC, identifier);
      return;
    }

  gimp_dialog_factory_set_widget_data (dialog, factory, entry);

  gimp_dialog_factory_add_dialog (factory, dialog);
}

void
gimp_dialog_factory_remove_dialog (GimpDialogFactory *factory,
                                   GtkWidget         *dialog)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;
  GList                  *list;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (GTK_IS_WIDGET (dialog));

  if (! g_list_find (factory->p->open_dialogs, dialog))
    {
      g_warning ("%s: dialog not registered", G_STRFUNC);
      return;
    }

  factory->p->open_dialogs = g_list_remove (factory->p->open_dialogs, dialog);

  dialog_factory = gimp_dialog_factory_from_widget (dialog, &entry);

  if (! gimp_dialog_factory_dialog_sane (factory,
                                         dialog_factory,
                                         entry,
                                         dialog))
    return;

  GIMP_LOG (DIALOG_FACTORY, "removing \"%s\" (dialog = %p)",
            entry->identifier,
            dialog);

  for (list = factory->p->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *session_info = list->data;

      if (gimp_session_info_get_widget (session_info) == dialog)
        {
          GIMP_LOG (DIALOG_FACTORY,
                    "clearing session info %p (widget %p) for \"%s\"",
                    session_info, gimp_session_info_get_widget (session_info),
                    entry->identifier);

          gimp_session_info_set_widget (session_info, NULL);

          gimp_dialog_factory_unset_widget_data (dialog);

          g_signal_handlers_disconnect_by_func (dialog,
                                                gimp_dialog_factory_set_user_pos,
                                                NULL);
          g_signal_handlers_disconnect_by_func (dialog,
                                                gimp_dialog_factory_remove_dialog,
                                                factory);

          if (gimp_session_info_is_session_managed (session_info))
            g_signal_handlers_disconnect_by_func (dialog,
                                                  gimp_dialog_factory_dialog_configure,
                                                  factory);

          if (GIMP_IS_DOCK_WINDOW (dialog))
            {
              /*  don't save session info for empty docks  */
              factory->p->session_infos = g_list_remove (factory->p->session_infos,
                                                      session_info);
              g_object_unref (session_info);

              g_signal_emit (factory, factory_signals[DOCK_WINDOW_REMOVED], 0,
                             dialog);
            }

          break;
        }
    }
}

void
gimp_dialog_factory_hide_dialog (GtkWidget *dialog)
{
  g_return_if_fail (GTK_IS_WIDGET (dialog));
  g_return_if_fail (gtk_widget_is_toplevel (dialog));

  if (! gimp_dialog_factory_from_widget (dialog, NULL))
    {
      g_warning ("%s: dialog was not created by a GimpDialogFactory",
                 G_STRFUNC);
      return;
    }

  gtk_widget_hide (dialog);

  if (dialogs_state != GIMP_DIALOGS_SHOWN)
    g_object_set_data (G_OBJECT (dialog), GIMP_DIALOG_VISIBILITY_KEY,
                       GINT_TO_POINTER (GIMP_DIALOG_VISIBILITY_INVISIBLE));
}

void
gimp_dialog_factories_session_save (GimpConfigWriter *writer)
{
  GimpDialogFactoryClass *factory_class;

  g_return_if_fail (writer != NULL);

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
                        (GHFunc) gimp_dialog_factories_save_foreach,
                        writer);
}

void
gimp_dialog_factories_session_restore (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
                        (GHFunc) gimp_dialog_factories_restore_foreach,
                        NULL);
}

void
gimp_dialog_factories_session_clear (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
                        (GHFunc) gimp_dialog_factories_clear_foreach,
                        NULL);
}

static void
gimp_dialog_factories_set_state (GimpDialogsState state)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  dialogs_state = state;

  if (state == GIMP_DIALOGS_SHOWN)
    {
      g_hash_table_foreach (factory_class->factories,
                            (GHFunc) gimp_dialog_factories_show_foreach,
                            NULL);
    }
  else
    {
      g_hash_table_foreach (factory_class->factories,
                            (GHFunc) gimp_dialog_factories_hide_foreach,
                            NULL);
    }
}

void
gimp_dialog_factories_show_with_display (void)
{
  if (dialogs_state == GIMP_DIALOGS_HIDDEN_WITH_DISPLAY)
    {
      gimp_dialog_factories_set_state (GIMP_DIALOGS_SHOWN);
    }
}

void
gimp_dialog_factories_hide_with_display (void)
{
  if (dialogs_state == GIMP_DIALOGS_SHOWN)
    {
      gimp_dialog_factories_set_state (GIMP_DIALOGS_HIDDEN_WITH_DISPLAY);
    }
}

void
gimp_dialog_factories_toggle (void)
{
  if (dialogs_state == GIMP_DIALOGS_SHOWN)
    {
      gimp_dialog_factories_set_state (GIMP_DIALOGS_HIDDEN_EXPLICITLY);
    }
  else
    {
      gimp_dialog_factories_set_state (GIMP_DIALOGS_SHOWN);
    }
}

void
gimp_dialog_factories_set_busy (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  if (factory_class)
    g_hash_table_foreach (factory_class->factories,
                          (GHFunc) gimp_dialog_factories_set_busy_foreach,
                          NULL);
}

void
gimp_dialog_factories_unset_busy (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  if (factory_class)
    g_hash_table_foreach (factory_class->factories,
                          (GHFunc) gimp_dialog_factories_unset_busy_foreach,
                          NULL);
}


static GQuark gimp_dialog_factory_key       = 0;
static GQuark gimp_dialog_factory_entry_key = 0;

GimpDialogFactory *
gimp_dialog_factory_from_widget (GtkWidget               *dialog,
                                 GimpDialogFactoryEntry **entry)
{
  g_return_val_if_fail (GTK_IS_WIDGET (dialog), NULL);

  if (! gimp_dialog_factory_key)
    {
      gimp_dialog_factory_key =
        g_quark_from_static_string ("gimp-dialog-factory");

      gimp_dialog_factory_entry_key =
        g_quark_from_static_string ("gimp-dialog-factory-entry");
    }

  if (entry)
    *entry = g_object_get_qdata (G_OBJECT (dialog),
                                 gimp_dialog_factory_entry_key);

  return g_object_get_qdata (G_OBJECT (dialog), gimp_dialog_factory_key);
}

#define GIMP_DIALOG_FACTORY_MIN_SIZE_KEY "gimp-dialog-factory-min-size"

void
gimp_dialog_factory_set_has_min_size (GtkWindow *window,
                                      gboolean   has_min_size)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  g_object_set_data (G_OBJECT (window), GIMP_DIALOG_FACTORY_MIN_SIZE_KEY,
                     GINT_TO_POINTER (has_min_size ? TRUE : FALSE));
}

gboolean
gimp_dialog_factory_get_has_min_size (GtkWindow *window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window),
                                             GIMP_DIALOG_FACTORY_MIN_SIZE_KEY));
}


/*  private functions  */

static GtkWidget *

gimp_dialog_factory_constructor (GimpDialogFactory      *factory,
                                 GimpDialogFactoryEntry *entry,
                                 GimpContext            *context,
                                 GimpUIManager          *ui_manager,
                                 gint                    view_size)
{
  GtkWidget *widget;

  widget = entry->new_func (factory, context, ui_manager, view_size);

  /* The entry is for a dockable, so we simply need to put the created
   * widget in a dockable
   */
  if (widget && entry->dockable)
    {
      GtkWidget *dockable = NULL;

      dockable = gimp_dockable_new (entry->name, entry->blurb,
                                    entry->stock_id, entry->help_id);
      gtk_container_add (GTK_CONTAINER (dockable), widget);
      gtk_widget_show (widget);

      /* EEK */
      g_object_set_data (G_OBJECT (dockable), "gimp-dialog-identifier",
                         entry->identifier);

      /* Return the dockable instead */
      widget = dockable;
    }

  return widget;
}

static void
gimp_dialog_factory_set_widget_data (GtkWidget               *dialog,
                                     GimpDialogFactory       *factory,
                                     GimpDialogFactoryEntry  *entry)
{
  g_return_if_fail (GTK_IS_WIDGET (dialog));
  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));

  if (! gimp_dialog_factory_key)
    {
      gimp_dialog_factory_key =
        g_quark_from_static_string ("gimp-dialog-factory");

      gimp_dialog_factory_entry_key =
        g_quark_from_static_string ("gimp-dialog-factory-entry");
    }

  g_object_set_qdata (G_OBJECT (dialog), gimp_dialog_factory_key, factory);

  if (entry)
    g_object_set_qdata (G_OBJECT (dialog), gimp_dialog_factory_entry_key,
                        entry);
}

static void
gimp_dialog_factory_unset_widget_data (GtkWidget *dialog)
{
  g_return_if_fail (GTK_IS_WIDGET (dialog));

  if (! gimp_dialog_factory_key)
    return;

  g_object_set_qdata (G_OBJECT (dialog), gimp_dialog_factory_key,       NULL);
  g_object_set_qdata (G_OBJECT (dialog), gimp_dialog_factory_entry_key, NULL);
}

static gboolean
gimp_dialog_factory_set_user_pos (GtkWidget         *dialog,
                                  GdkEventConfigure *cevent,
                                  gpointer           data)
{
  GdkWindowHints          geometry_mask;
#ifdef DEBUG_FACTORY
  GimpDialogFactoryEntry *entry;

  gimp_dialog_factory_from_widget (dialog, &entry);

  if (entry)
    g_print ("%s: setting GDK_HINT_USER_POS for \"%s\"\n",
             G_STRFUNC, entry->identifier);
#endif /* DEBUG_FACTORY */

  g_signal_handlers_disconnect_by_func (dialog,
                                        gimp_dialog_factory_set_user_pos,
                                        data);

  geometry_mask = GDK_HINT_USER_POS;

  if (gimp_dialog_factory_get_has_min_size (GTK_WINDOW (dialog)))
    geometry_mask |= GDK_HINT_MIN_SIZE;

  gtk_window_set_geometry_hints (GTK_WINDOW (dialog), NULL, NULL,
                                 geometry_mask);

  return FALSE;
}

static gboolean
gimp_dialog_factory_dialog_configure (GtkWidget         *dialog,
                                      GdkEventConfigure *cevent,
                                      GimpDialogFactory *factory)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;
  GList                  *list;

  if (! g_list_find (factory->p->open_dialogs, dialog))
    {
      g_warning ("%s: dialog not registered", G_STRFUNC);
      return FALSE;
    }

  dialog_factory = gimp_dialog_factory_from_widget (dialog, &entry);

  if (! gimp_dialog_factory_dialog_sane (factory,
                                         dialog_factory,
                                         entry,
                                         dialog))
    return FALSE;

  for (list = factory->p->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *session_info = list->data;

      if (gimp_session_info_get_widget (session_info) == dialog)
        {
          gimp_session_info_read_geometry (session_info);

          GIMP_LOG (DIALOG_FACTORY,
                    "updated session info for \"%s\" from window geometry "
                    "(x=%d y=%d  %dx%d)",
                    entry->identifier,
                    gimp_session_info_get_x (session_info),
                    gimp_session_info_get_y (session_info),
                    gimp_session_info_get_width (session_info),
                    gimp_session_info_get_height (session_info));

          break;
        }
    }

  return FALSE;
}

static void
gimp_dialog_factories_save_foreach (gconstpointer      key,
                                    GimpDialogFactory *factory,
                                    GimpConfigWriter  *writer)
{
  GList *infos;

  for (infos = factory->p->session_infos; infos; infos = g_list_next (infos))
    {
      GimpSessionInfo *info       = infos->data;
      const gchar     *identifier = NULL;

      /*  we keep session info entries for all toplevel dialogs created
       *  by the factory but don't save them if they don't want to be
       *  managed
       */
      if (! gimp_session_info_is_session_managed (info) ||
          gimp_session_info_get_factory_entry (info) == NULL)
        continue;

      if (gimp_session_info_get_widget (info))
        gimp_session_info_get_info (info);

      gimp_config_writer_open (writer, "session-info");
      gimp_config_writer_string (writer,
                                 gimp_object_get_name (factory));

      /* Some special casing to be avoid changing sessionrc */
      identifier = gimp_session_info_get_factory_entry (info)->identifier;
      if (strcmp (identifier, "gimp-dock-window")    == 0 ||
          strcmp (identifier, "gimp-toolbox-window") == 0)
        identifier = "dock";
      gimp_config_writer_string (writer, identifier);

      GIMP_CONFIG_GET_INTERFACE (info)->serialize (GIMP_CONFIG (info),
                                                   writer,
                                                   NULL);

      gimp_config_writer_close (writer);

      if (gimp_session_info_get_widget (info))
        gimp_session_info_clear_info (info);
    }
}

static void
gimp_dialog_factories_restore_foreach (gconstpointer      key,
                                       GimpDialogFactory *factory,
                                       gpointer           data)
{
  GList *infos;

  for (infos = factory->p->session_infos; infos; infos = g_list_next (infos))
    {
      GimpSessionInfo *info = infos->data;

      if (gimp_session_info_get_open (info))
        {
          gimp_session_info_restore (info, factory);
          gimp_session_info_clear_info (info);
        }
      else
        {
          GIMP_LOG (DIALOG_FACTORY,
                    "skipping to restore session info %p, not open",
                    info);
        }
    }
}

static void
gimp_dialog_factories_clear_foreach (gconstpointer      key,
                                     GimpDialogFactory *factory,
                                     gpointer           data)
{
  GList *list;

  for (list = factory->p->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *info = list->data;

      if (gimp_session_info_get_widget (info))
        continue;

      /* FIXME: implement session info deletion */
    }
}

static void
gimp_dialog_factories_hide_foreach (gconstpointer      key,
                                    GimpDialogFactory *factory,
                                    gpointer           data)
{
  GList *list;

  if (! factory->p->toggle_visibility)
    return;

  for (list = factory->p->open_dialogs; list; list = g_list_next (list))
    {
      GtkWidget *widget = list->data;

      if (GTK_IS_WIDGET (widget) && gtk_widget_is_toplevel (widget))
        {
          GimpDialogVisibilityState visibility = GIMP_DIALOG_VISIBILITY_UNKNOWN;

          if (gtk_widget_get_visible (widget))
            {
              visibility = GIMP_DIALOG_VISIBILITY_VISIBLE;

              gtk_widget_hide (widget);

              GIMP_LOG (WM, "Hiding '%s' [%p]",
                        gtk_window_get_title (GTK_WINDOW (widget)),
                        widget);
            }
          else
            {
              visibility = GIMP_DIALOG_VISIBILITY_INVISIBLE;
            }

          g_object_set_data (G_OBJECT (widget),
                             GIMP_DIALOG_VISIBILITY_KEY,
                             GINT_TO_POINTER (visibility));
        }
    }
}

static void
gimp_dialog_factories_show_foreach (gconstpointer      key,
                                    GimpDialogFactory *factory,
                                    gpointer           data)
{
  GList *list;

  for (list = factory->p->open_dialogs; list; list = g_list_next (list))
    {
      GtkWidget *widget = list->data;

      if (GTK_IS_WIDGET (widget) && gtk_widget_is_toplevel (widget))
        {
          GimpDialogVisibilityState visibility;

          visibility =
            GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                GIMP_DIALOG_VISIBILITY_KEY));

          if (! gtk_widget_get_visible (widget) &&
              visibility == GIMP_DIALOG_VISIBILITY_VISIBLE)
            {
              GIMP_LOG (WM, "Showing '%s' [%p]",
                        gtk_window_get_title (GTK_WINDOW (widget)),
                        widget);

              /* Don't use gtk_window_present() here, we don't want the
               * keyboard focus to move.
               */
              gtk_widget_show (widget);

              if (gtk_widget_get_visible (widget))
                gdk_window_raise (gtk_widget_get_window (widget));
            }
        }
    }
}

static void
gimp_dialog_factories_set_busy_foreach (gconstpointer      key,
                                        GimpDialogFactory *factory,
                                        gpointer           data)
{
  GdkDisplay *display = NULL;
  GdkCursor  *cursor  = NULL;
  GList      *list;

  for (list = factory->p->open_dialogs; list; list = g_list_next (list))
    {
      GtkWidget *widget = list->data;

      if (GTK_IS_WIDGET (widget) && gtk_widget_is_toplevel (widget))
        {
          if (!display || display != gtk_widget_get_display (widget))
            {
              display = gtk_widget_get_display (widget);

              if (cursor)
                gdk_cursor_unref (cursor);

              cursor = gimp_cursor_new (display,
                                        GIMP_CURSOR_FORMAT_BITMAP,
                                        GDK_WATCH,
                                        GIMP_TOOL_CURSOR_NONE,
                                        GIMP_CURSOR_MODIFIER_NONE);
            }

          if (gtk_widget_get_window (widget))
            gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
        }
    }

  if (cursor)
    gdk_cursor_unref (cursor);
}

static void
gimp_dialog_factories_unset_busy_foreach (gconstpointer      key,
                                          GimpDialogFactory *factory,
                                          gpointer           data)
{
  GList *list;

  for (list = factory->p->open_dialogs; list; list = g_list_next (list))
    {
      GtkWidget *widget = list->data;

      if (GTK_IS_WIDGET (widget) && gtk_widget_is_toplevel (widget))
        {
          if (gtk_widget_get_window (widget))
            gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
        }
    }
}
