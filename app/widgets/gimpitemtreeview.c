/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpitemtreeview.c
 * Copyright (C) 2001-2009 Michael Natterer <mitch@gimp.org>
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

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-undo-push.h"
#include "core/gimpitemundo.h"
#include "core/gimplayer.h"
#include "core/gimpmarshal.h"
#include "core/gimptreehandler.h"
#include "core/gimpundostack.h"

#include "vectors/gimpvectors.h"

#include "gimpcontainerview.h"
#include "gimpdnd.h"
#include "gimpdocked.h"
#include "gimpitemtreeview.h"
#include "gimpmenufactory.h"
#include "gimpviewrenderer.h"
#include "gimpuimanager.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"


enum
{
  SET_IMAGE,
  LAST_SIGNAL
};


struct _GimpItemTreeViewPriv
{
  GimpImage       *image;

  GtkWidget       *edit_button;
  GtkWidget       *new_button;
  GtkWidget       *raise_button;
  GtkWidget       *lower_button;
  GtkWidget       *duplicate_button;
  GtkWidget       *delete_button;

  gint             model_column_visible;
  gint             model_column_linked;
  GtkCellRenderer *eye_cell;
  GtkCellRenderer *chain_cell;

  GimpTreeHandler *visible_changed_handler;
  GimpTreeHandler *linked_changed_handler;
};


static void   gimp_item_tree_view_view_iface_init   (GimpContainerViewInterface *view_iface);
static void   gimp_item_tree_view_docked_iface_init (GimpDockedInterface *docked_iface);

static GObject * gimp_item_tree_view_constructor    (GType              type,
                                                     guint              n_params,
                                                     GObjectConstructParam *params);

static void   gimp_item_tree_view_destroy           (GtkObject         *object);

static void   gimp_item_tree_view_real_set_image    (GimpItemTreeView  *view,
                                                     GimpImage         *image);

static void   gimp_item_tree_view_image_flush       (GimpImage         *image,
                                                     gboolean           invalidate_preview,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_set_container     (GimpContainerView *view,
                                                     GimpContainer     *container);
static void   gimp_item_tree_view_set_context       (GimpContainerView *view,
                                                     GimpContext       *context);

static gpointer gimp_item_tree_view_insert_item     (GimpContainerView *view,
                                                     GimpViewable      *viewable,
                                                     gpointer           parent_insert_data,
                                                     gint               index);
static gboolean gimp_item_tree_view_select_item     (GimpContainerView *view,
                                                     GimpViewable      *item,
                                                     gpointer           insert_data);
static void   gimp_item_tree_view_activate_item     (GimpContainerView *view,
                                                     GimpViewable      *item,
                                                     gpointer           insert_data);
static void   gimp_item_tree_view_context_item      (GimpContainerView *view,
                                                     GimpViewable      *item,
                                                     gpointer           insert_data);

static gboolean gimp_item_tree_view_drop_possible   (GimpContainerTreeView *view,
                                                     GimpDndType        src_type,
                                                     GimpViewable      *src_viewable,
                                                     GimpViewable      *dest_viewable,
                                                     GtkTreeViewDropPosition  drop_pos,
                                                     GtkTreeViewDropPosition *return_drop_pos,
                                                     GdkDragAction     *return_drag_action);
static void     gimp_item_tree_view_drop_viewable   (GimpContainerTreeView *view,
                                                     GimpViewable      *src_viewable,
                                                     GimpViewable      *dest_viewable,
                                                     GtkTreeViewDropPosition  drop_pos);

static void   gimp_item_tree_view_new_dropped       (GtkWidget         *widget,
                                                     gint               x,
                                                     gint               y,
                                                     GimpViewable      *viewable,
                                                     gpointer           data);

static void   gimp_item_tree_view_item_changed      (GimpImage         *image,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_size_changed      (GimpImage         *image,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_name_edited       (GtkCellRendererText *cell,
                                                     const gchar       *path,
                                                     const gchar       *new_name,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_visible_changed   (GimpItem          *item,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_linked_changed    (GimpItem          *item,
                                                     GimpItemTreeView  *view);

static void   gimp_item_tree_view_eye_clicked       (GtkCellRendererToggle *toggle,
                                                     gchar             *path,
                                                     GdkModifierType    state,
                                                     GimpItemTreeView  *view);
static void   gimp_item_tree_view_chain_clicked     (GtkCellRendererToggle *toggle,
                                                     gchar             *path,
                                                     GdkModifierType    state,
                                                     GimpItemTreeView  *view);

/*  utility function to avoid code duplication  */
static void   gimp_item_tree_view_toggle_clicked    (GtkCellRendererToggle *toggle,
                                                     gchar             *path_str,
                                                     GdkModifierType    state,
                                                     GimpItemTreeView  *view,
                                                     GimpUndoType       undo_type);


G_DEFINE_TYPE_WITH_CODE (GimpItemTreeView, gimp_item_tree_view,
                         GIMP_TYPE_CONTAINER_TREE_VIEW,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_CONTAINER_VIEW,
                                                gimp_item_tree_view_view_iface_init)
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_DOCKED,
                                                gimp_item_tree_view_docked_iface_init))

#define parent_class gimp_item_tree_view_parent_class

static GimpContainerViewInterface *parent_view_iface = NULL;

static guint view_signals[LAST_SIGNAL] = { 0 };


static void
gimp_item_tree_view_class_init (GimpItemTreeViewClass *klass)
{
  GObjectClass               *object_class;
  GtkObjectClass             *gtk_object_class;
  GimpContainerTreeViewClass *tree_view_class;

  object_class     = G_OBJECT_CLASS (klass);
  gtk_object_class = GTK_OBJECT_CLASS (klass);
  tree_view_class  = GIMP_CONTAINER_TREE_VIEW_CLASS (klass);

  view_signals[SET_IMAGE] =
    g_signal_new ("set-image",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GimpItemTreeViewClass, set_image),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_OBJECT);

  object_class->constructor      = gimp_item_tree_view_constructor;

  gtk_object_class->destroy      = gimp_item_tree_view_destroy;

  tree_view_class->drop_possible = gimp_item_tree_view_drop_possible;
  tree_view_class->drop_viewable = gimp_item_tree_view_drop_viewable;

  klass->set_image               = gimp_item_tree_view_real_set_image;

  klass->item_type               = G_TYPE_NONE;
  klass->signal_name             = NULL;

  klass->get_container           = NULL;
  klass->get_active_item         = NULL;
  klass->set_active_item         = NULL;
  klass->reorder_item            = NULL;
  klass->add_item                = NULL;
  klass->remove_item             = NULL;
  klass->new_item                = NULL;

  klass->action_group            = NULL;
  klass->edit_action             = NULL;
  klass->new_action              = NULL;
  klass->new_default_action      = NULL;
  klass->raise_action            = NULL;
  klass->raise_top_action        = NULL;
  klass->lower_action            = NULL;
  klass->lower_bottom_action     = NULL;
  klass->duplicate_action        = NULL;
  klass->delete_action           = NULL;
  klass->reorder_desc            = NULL;

  g_type_class_add_private (klass, sizeof (GimpItemTreeViewPriv));
}

static void
gimp_item_tree_view_view_iface_init (GimpContainerViewInterface *iface)
{
  parent_view_iface = g_type_interface_peek_parent (iface);

  iface->set_container = gimp_item_tree_view_set_container;
  iface->set_context   = gimp_item_tree_view_set_context;
  iface->insert_item   = gimp_item_tree_view_insert_item;
  iface->select_item   = gimp_item_tree_view_select_item;
  iface->activate_item = gimp_item_tree_view_activate_item;
  iface->context_item  = gimp_item_tree_view_context_item;
}

static void
gimp_item_tree_view_docked_iface_init (GimpDockedInterface *iface)
{
  iface->get_preview = NULL;
}

static void
gimp_item_tree_view_init (GimpItemTreeView *view)
{
  GimpContainerTreeView *tree_view = GIMP_CONTAINER_TREE_VIEW (view);

  view->priv = G_TYPE_INSTANCE_GET_PRIVATE (view,
                                            GIMP_TYPE_ITEM_TREE_VIEW,
                                            GimpItemTreeViewPriv);

  /* The following used to read:
   *
   * tree_view->model_columns[tree_view->n_model_columns++] = ...
   *
   * but combining the two lead to gcc miscompiling the function on ppc/ia64
   * (model_column_mask and model_column_mask_visible would have the same
   * value, probably due to bad instruction reordering). See bug #113144 for
   * more info.
   */
  view->priv->model_column_visible = tree_view->n_model_columns;
  tree_view->model_columns[tree_view->n_model_columns] = G_TYPE_BOOLEAN;
  tree_view->n_model_columns++;

  view->priv->model_column_linked = tree_view->n_model_columns;
  tree_view->model_columns[tree_view->n_model_columns] = G_TYPE_BOOLEAN;
  tree_view->n_model_columns++;

  gimp_container_tree_view_set_dnd_drop_to_empty (tree_view, TRUE);

  view->priv->image  = NULL;
}

static GObject *
gimp_item_tree_view_constructor (GType                  type,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GimpItemTreeViewClass *item_view_class;
  GimpEditor            *editor;
  GimpContainerTreeView *tree_view;
  GimpItemTreeView      *item_view;
  GObject               *object;
  GtkTreeViewColumn     *column;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  editor          = GIMP_EDITOR (object);
  tree_view       = GIMP_CONTAINER_TREE_VIEW (object);
  item_view       = GIMP_ITEM_TREE_VIEW (object);
  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (object);

  gimp_container_tree_view_connect_name_edited (tree_view,
                                                G_CALLBACK (gimp_item_tree_view_name_edited),
                                                item_view);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_insert_column (tree_view->view, column, 0);

  item_view->priv->eye_cell = gimp_cell_renderer_toggle_new (GIMP_STOCK_VISIBLE);
  gtk_tree_view_column_pack_start (column, item_view->priv->eye_cell, FALSE);
  gtk_tree_view_column_set_attributes (column, item_view->priv->eye_cell,
                                       "active",
                                       item_view->priv->model_column_visible,
                                       NULL);

  gimp_container_tree_view_prepend_toggle_cell_renderer (tree_view,
                                                         item_view->priv->eye_cell);

  g_signal_connect (item_view->priv->eye_cell, "clicked",
                    G_CALLBACK (gimp_item_tree_view_eye_clicked),
                    item_view);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_insert_column (tree_view->view, column, 1);

  item_view->priv->chain_cell = gimp_cell_renderer_toggle_new (GIMP_STOCK_LINKED);
  gtk_tree_view_column_pack_start (column, item_view->priv->chain_cell, FALSE);
  gtk_tree_view_column_set_attributes (column, item_view->priv->chain_cell,
                                       "active",
                                       item_view->priv->model_column_linked,
                                       NULL);

  gimp_container_tree_view_prepend_toggle_cell_renderer (tree_view,
                                                         item_view->priv->chain_cell);

  g_signal_connect (item_view->priv->chain_cell, "clicked",
                    G_CALLBACK (gimp_item_tree_view_chain_clicked),
                    item_view);

  /*  disable the default GimpContainerView drop handler  */
  gimp_container_view_set_dnd_widget (GIMP_CONTAINER_VIEW (item_view), NULL);

  gimp_dnd_drag_dest_set_by_type (GTK_WIDGET (tree_view->view),
                                  GTK_DEST_DEFAULT_HIGHLIGHT,
                                  item_view_class->item_type,
                                  GDK_ACTION_MOVE | GDK_ACTION_COPY);

  item_view->priv->edit_button =
    gimp_editor_add_action_button (editor, item_view_class->action_group,
                                   item_view_class->edit_action, NULL);
  gimp_container_view_enable_dnd (GIMP_CONTAINER_VIEW (item_view),
                                  GTK_BUTTON (item_view->priv->edit_button),
                                  item_view_class->item_type);

  item_view->priv->new_button =
    gimp_editor_add_action_button (editor, item_view_class->action_group,
                                   item_view_class->new_action,
                                   item_view_class->new_default_action,
                                   GDK_SHIFT_MASK,
                                   NULL);
  /*  connect "drop to new" manually as it makes a difference whether
   *  it was clicked or dropped
   */
  gimp_dnd_viewable_dest_add (item_view->priv->new_button,
                              item_view_class->item_type,
                              gimp_item_tree_view_new_dropped,
                              item_view);

  item_view->priv->raise_button =
    gimp_editor_add_action_button (editor, item_view_class->action_group,
                                   item_view_class->raise_action,
                                   item_view_class->raise_top_action,
                                   GDK_SHIFT_MASK,
                                   NULL);

  item_view->priv->lower_button =
    gimp_editor_add_action_button (editor, item_view_class->action_group,
                                   item_view_class->lower_action,
                                   item_view_class->lower_bottom_action,
                                   GDK_SHIFT_MASK,
                                   NULL);

  item_view->priv->duplicate_button =
    gimp_editor_add_action_button (editor, item_view_class->action_group,
                                   item_view_class->duplicate_action, NULL);
  gimp_container_view_enable_dnd (GIMP_CONTAINER_VIEW (item_view),
                                  GTK_BUTTON (item_view->priv->duplicate_button),
                                  item_view_class->item_type);

  item_view->priv->delete_button =
    gimp_editor_add_action_button (editor, item_view_class->action_group,
                                   item_view_class->delete_action, NULL);
  gimp_container_view_enable_dnd (GIMP_CONTAINER_VIEW (item_view),
                                  GTK_BUTTON (item_view->priv->delete_button),
                                  item_view_class->item_type);

  return object;
}

static void
gimp_item_tree_view_destroy (GtkObject *object)
{
  GimpItemTreeView *view = GIMP_ITEM_TREE_VIEW (object);

  if (view->priv->image)
    gimp_item_tree_view_set_image (view, NULL);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
gimp_item_tree_view_new (GType            view_type,
                         gint             view_size,
                         gint             view_border_width,
                         GimpImage       *image,
                         GimpMenuFactory *menu_factory,
                         const gchar     *menu_identifier,
                         const gchar     *ui_path)
{
  GimpItemTreeView *item_view;

  g_return_val_if_fail (g_type_is_a (view_type, GIMP_TYPE_ITEM_TREE_VIEW), NULL);
  g_return_val_if_fail (view_size >  0 &&
                        view_size <= GIMP_VIEWABLE_MAX_PREVIEW_SIZE, NULL);
  g_return_val_if_fail (view_border_width >= 0 &&
                        view_border_width <= GIMP_VIEW_MAX_BORDER_WIDTH,
                        NULL);
  g_return_val_if_fail (image == NULL || GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_MENU_FACTORY (menu_factory), NULL);
  g_return_val_if_fail (menu_identifier != NULL, NULL);
  g_return_val_if_fail (ui_path != NULL, NULL);

  item_view = g_object_new (view_type,
                            "reorderable",     TRUE,
                            "menu-factory",    menu_factory,
                            "menu-identifier", menu_identifier,
                            "ui-path",         ui_path,
                            NULL);

  gimp_container_view_set_view_size (GIMP_CONTAINER_VIEW (item_view),
                                     view_size, view_border_width);

  gimp_item_tree_view_set_image (item_view, image);

  return GTK_WIDGET (item_view);
}

void
gimp_item_tree_view_set_image (GimpItemTreeView *view,
                               GimpImage        *image)
{
  g_return_if_fail (GIMP_IS_ITEM_TREE_VIEW (view));
  g_return_if_fail (image == NULL || GIMP_IS_IMAGE (image));

  g_signal_emit (view, view_signals[SET_IMAGE], 0, image);

  gimp_ui_manager_update (GIMP_EDITOR (view)->ui_manager, view);
}

GimpImage *
gimp_item_tree_view_get_image (GimpItemTreeView *view)
{
  g_return_val_if_fail (GIMP_IS_ITEM_TREE_VIEW (view), NULL);

  return view->priv->image;
}

GtkWidget *
gimp_item_tree_view_get_new_button (GimpItemTreeView *view)
{
  g_return_val_if_fail (GIMP_IS_ITEM_TREE_VIEW (view), NULL);

  return view->priv->new_button;
}

GtkWidget *
gimp_item_tree_view_get_edit_button (GimpItemTreeView *view)
{
  g_return_val_if_fail (GIMP_IS_ITEM_TREE_VIEW (view), NULL);

  return view->priv->edit_button;
}

gint
gimp_item_tree_view_get_drop_index (GimpItemTreeView         *view,
                                    GimpViewable             *dest_viewable,
                                    GtkTreeViewDropPosition   drop_pos,
                                    GimpViewable            **parent)
{
  gint index = -1;

  g_return_val_if_fail (GIMP_IS_ITEM_TREE_VIEW (view), -1);
  g_return_val_if_fail (dest_viewable == NULL ||
                        GIMP_IS_VIEWABLE (dest_viewable), -1);
  g_return_val_if_fail (parent != NULL, -1);

  *parent = NULL;

  if (dest_viewable)
    {
      *parent = gimp_viewable_get_parent (dest_viewable);
      index   = gimp_item_get_index (GIMP_ITEM (dest_viewable));

      if (drop_pos == GTK_TREE_VIEW_DROP_AFTER)
        {
          GimpContainer *children = gimp_viewable_get_children (dest_viewable);

          if (children)
            {
              *parent = dest_viewable;
              index   = 0;
            }
          else
            {
              index++;
            }
        }
    }

  return index;
}

static void
gimp_item_tree_view_real_set_image (GimpItemTreeView *view,
                                    GimpImage        *image)
{
  if (view->priv->image == image)
    return;

  if (view->priv->image)
    {
      g_signal_handlers_disconnect_by_func (view->priv->image,
                                            gimp_item_tree_view_item_changed,
                                            view);
      g_signal_handlers_disconnect_by_func (view->priv->image,
                                            gimp_item_tree_view_size_changed,
                                            view);

      gimp_container_view_set_container (GIMP_CONTAINER_VIEW (view), NULL);

      g_signal_handlers_disconnect_by_func (view->priv->image,
                                            gimp_item_tree_view_image_flush,
                                            view);
    }

  view->priv->image = image;

  if (view->priv->image)
    {
      GimpContainer *container;

      container =
        GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_container (view->priv->image);

      gimp_container_view_set_container (GIMP_CONTAINER_VIEW (view), container);

      g_signal_connect (view->priv->image,
                        GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->signal_name,
                        G_CALLBACK (gimp_item_tree_view_item_changed),
                        view);
      g_signal_connect (view->priv->image, "size-changed",
                        G_CALLBACK (gimp_item_tree_view_size_changed),
                        view);

      g_signal_connect (view->priv->image, "flush",
                        G_CALLBACK (gimp_item_tree_view_image_flush),
                        view);

      gimp_item_tree_view_item_changed (view->priv->image, view);
    }
}

static void
gimp_item_tree_view_image_flush (GimpImage        *image,
                                 gboolean          invalidate_preview,
                                 GimpItemTreeView *view)
{
  gimp_ui_manager_update (GIMP_EDITOR (view)->ui_manager, view);
}


/*  GimpContainerView methods  */

static void
gimp_item_tree_view_set_container (GimpContainerView *view,
                                   GimpContainer     *container)
{
  GimpItemTreeView *item_view = GIMP_ITEM_TREE_VIEW (view);
  GimpContainer    *old_container;

  old_container = gimp_container_view_get_container (view);

  if (old_container)
    {
      gimp_tree_handler_disconnect (item_view->priv->visible_changed_handler);
      item_view->priv->visible_changed_handler = NULL;

      gimp_tree_handler_disconnect (item_view->priv->linked_changed_handler);
      item_view->priv->linked_changed_handler = NULL;
    }

  parent_view_iface->set_container (view, container);

  if (container)
    {
      item_view->priv->visible_changed_handler =
        gimp_tree_handler_connect (container, "visibility-changed",
                                   G_CALLBACK (gimp_item_tree_view_visible_changed),
                                   view);

      item_view->priv->linked_changed_handler =
        gimp_tree_handler_connect (container, "linked-changed",
                                   G_CALLBACK (gimp_item_tree_view_linked_changed),
                                   view);
    }
}

static void
gimp_item_tree_view_set_context (GimpContainerView *view,
                                 GimpContext       *context)
{
  GimpContainerTreeView *tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  GimpItemTreeView      *item_view = GIMP_ITEM_TREE_VIEW (view);
  GimpImage             *image     = NULL;
  GimpContext           *old_context;

  old_context = gimp_container_view_get_context (view);

  if (old_context)
    {
      g_signal_handlers_disconnect_by_func (old_context,
                                            gimp_item_tree_view_set_image,
                                            item_view);
    }

  parent_view_iface->set_context (view, context);

  if (context)
    {
      if (! tree_view->dnd_gimp)
        tree_view->dnd_gimp = context->gimp;

      g_signal_connect_swapped (context, "image-changed",
                                G_CALLBACK (gimp_item_tree_view_set_image),
                                item_view);

      image = gimp_context_get_image (context);
    }

  gimp_item_tree_view_set_image (item_view, image);
}

static gpointer
gimp_item_tree_view_insert_item (GimpContainerView *view,
                                 GimpViewable      *viewable,
                                 gpointer           parent_insert_data,
                                 gint               index)
{
  GimpContainerTreeView *tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  GimpItemTreeView      *item_view = GIMP_ITEM_TREE_VIEW (view);
  GimpItem              *item      = GIMP_ITEM (viewable);
  GtkTreeIter           *iter;

  iter = parent_view_iface->insert_item (view, viewable,
                                         parent_insert_data, index);

  gtk_tree_store_set (GTK_TREE_STORE (tree_view->model), iter,
                      item_view->priv->model_column_visible,
                      gimp_item_get_visible (item),
                      item_view->priv->model_column_linked,
                      gimp_item_get_linked (item),
                      -1);

  return iter;
}

static gboolean
gimp_item_tree_view_select_item (GimpContainerView *view,
                                 GimpViewable      *item,
                                 gpointer           insert_data)
{
  GimpItemTreeView *tree_view = GIMP_ITEM_TREE_VIEW (view);
  gboolean          success;

  success = parent_view_iface->select_item (view, item, insert_data);

  if (item)
    {
      GimpItemTreeViewClass *item_view_class;
      GimpItem              *active_item;

      item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (tree_view);

      active_item = item_view_class->get_active_item (tree_view->priv->image);

      if (active_item != (GimpItem *) item)
        {
          item_view_class->set_active_item (tree_view->priv->image,
                                            GIMP_ITEM (item));

          gimp_image_flush (tree_view->priv->image);
        }
    }

  gimp_ui_manager_update (GIMP_EDITOR (tree_view)->ui_manager, tree_view);

  return success;
}

static void
gimp_item_tree_view_activate_item (GimpContainerView *view,
                                   GimpViewable      *item,
                                   gpointer           insert_data)
{
  GimpItemTreeViewClass *item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

  if (parent_view_iface->activate_item)
    parent_view_iface->activate_item (view, item, insert_data);

  if (item_view_class->activate_action)
    {
      gimp_ui_manager_activate_action (GIMP_EDITOR (view)->ui_manager,
                                       item_view_class->action_group,
                                       item_view_class->activate_action);
    }
}

static void
gimp_item_tree_view_context_item (GimpContainerView *view,
                                  GimpViewable      *item,
                                  gpointer           insert_data)
{
  if (parent_view_iface->context_item)
    parent_view_iface->context_item (view, item, insert_data);

  gimp_editor_popup_menu (GIMP_EDITOR (view), NULL, NULL);
}

static gboolean
gimp_item_tree_view_drop_possible (GimpContainerTreeView   *tree_view,
                                   GimpDndType              src_type,
                                   GimpViewable            *src_viewable,
                                   GimpViewable            *dest_viewable,
                                   GtkTreeViewDropPosition  drop_pos,
                                   GtkTreeViewDropPosition *return_drop_pos,
                                   GdkDragAction           *return_drag_action)
{
  if (GIMP_IS_ITEM (src_viewable) &&
      (dest_viewable == NULL ||
       gimp_item_get_image (GIMP_ITEM (src_viewable)) !=
       gimp_item_get_image (GIMP_ITEM (dest_viewable))))
    {
      if (return_drop_pos)
        *return_drop_pos = drop_pos;

      if (return_drag_action)
        *return_drag_action = GDK_ACTION_COPY;

      return TRUE;
    }

  return GIMP_CONTAINER_TREE_VIEW_CLASS (parent_class)->drop_possible (tree_view,
                                                                       src_type,
                                                                       src_viewable,
                                                                       dest_viewable,
                                                                       drop_pos,
                                                                       return_drop_pos,
                                                                       return_drag_action);
}

static void
gimp_item_tree_view_drop_viewable (GimpContainerTreeView   *tree_view,
                                   GimpViewable            *src_viewable,
                                   GimpViewable            *dest_viewable,
                                   GtkTreeViewDropPosition  drop_pos)
{
  GimpContainerView     *container_view = GIMP_CONTAINER_VIEW (tree_view);
  GimpItemTreeView      *item_view      = GIMP_ITEM_TREE_VIEW (tree_view);
  GimpItemTreeViewClass *item_view_class;
  GimpContainer         *container;
  gint                   dest_index     = -1;

  container = gimp_container_view_get_container (container_view);

  if (dest_viewable)
    dest_index = gimp_container_get_child_index (container,
                                                 GIMP_OBJECT (dest_viewable));

  item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (item_view);

  if (item_view->priv->image != gimp_item_get_image (GIMP_ITEM (src_viewable)) ||
      ! g_type_is_a (G_TYPE_FROM_INSTANCE (src_viewable),
                     item_view_class->item_type))
    {
      GType     item_type = item_view_class->item_type;
      GimpItem *new_item;
      GimpItem *parent;

      if (g_type_is_a (G_TYPE_FROM_INSTANCE (src_viewable), item_type))
        item_type = G_TYPE_FROM_INSTANCE (src_viewable);

      dest_index = gimp_item_tree_view_get_drop_index (item_view, dest_viewable,
                                                       drop_pos,
                                                       (GimpViewable **) &parent);

      new_item = gimp_item_convert (GIMP_ITEM (src_viewable),
                                    item_view->priv->image, item_type);

      gimp_item_set_linked (new_item, FALSE, FALSE);

      item_view_class->add_item (item_view->priv->image, new_item,
                                 parent, dest_index, TRUE);
    }
  else if (dest_viewable)
    {
      gint src_index;

      src_index = gimp_container_get_child_index (container,
                                                  GIMP_OBJECT (src_viewable));

      if (drop_pos == GTK_TREE_VIEW_DROP_AFTER && src_index > dest_index)
        {
          dest_index++;
        }
      else if (drop_pos == GTK_TREE_VIEW_DROP_BEFORE && src_index < dest_index)
        {
          dest_index--;
        }

      item_view_class->reorder_item (item_view->priv->image,
                                     GIMP_ITEM (src_viewable),
                                     dest_index,
                                     TRUE,
                                     item_view_class->reorder_desc);
    }

  gimp_image_flush (item_view->priv->image);
}


/*  "New" functions  */

static void
gimp_item_tree_view_new_dropped (GtkWidget    *widget,
                                 gint          x,
                                 gint          y,
                                 GimpViewable *viewable,
                                 gpointer      data)
{
  GimpItemTreeViewClass *item_view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (data);
  GimpItemTreeView      *view            = GIMP_ITEM_TREE_VIEW (data);
  GimpContainer         *container;

  container = gimp_container_view_get_container (GIMP_CONTAINER_VIEW (view));

  if (viewable && gimp_container_have (container, GIMP_OBJECT (viewable)) &&
      item_view_class->new_default_action)
    {
      GtkAction *action;

      action = gimp_ui_manager_find_action (GIMP_EDITOR (view)->ui_manager,
                                            item_view_class->action_group,
                                            item_view_class->new_default_action);

      if (action)
        {
          g_object_set (action, "viewable", viewable, NULL);
          gtk_action_activate (action);
          g_object_set (action, "viewable", NULL, NULL);
        }
    }
}


/*  GimpImage callbacks  */

static void
gimp_item_tree_view_item_changed (GimpImage        *image,
                                  GimpItemTreeView *view)
{
  GimpItem *item;

  item = GIMP_ITEM_TREE_VIEW_GET_CLASS (view)->get_active_item (view->priv->image);

  gimp_container_view_select_item (GIMP_CONTAINER_VIEW (view),
                                   (GimpViewable *) item);
}

static void
gimp_item_tree_view_size_changed (GimpImage        *image,
                                  GimpItemTreeView *tree_view)
{
  GimpContainerView *view = GIMP_CONTAINER_VIEW (tree_view);
  gint               view_size;
  gint               border_width;

  view_size = gimp_container_view_get_view_size (view, &border_width);

  gimp_container_view_set_view_size (view, view_size, border_width);
}

static void
gimp_item_tree_view_name_edited (GtkCellRendererText *cell,
                                 const gchar         *path_str,
                                 const gchar         *new_name,
                                 GimpItemTreeView    *view)
{
  GimpContainerTreeView *tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  GtkTreePath           *path;
  GtkTreeIter            iter;

  path = gtk_tree_path_new_from_string (path_str);

  if (gtk_tree_model_get_iter (tree_view->model, &iter, path))
    {
      GimpViewRenderer *renderer;
      GimpItem         *item;
      const gchar      *old_name;
      GError           *error = NULL;

      gtk_tree_model_get (tree_view->model, &iter,
                          GIMP_CONTAINER_TREE_VIEW_COLUMN_RENDERER, &renderer,
                          -1);

      item = GIMP_ITEM (renderer->viewable);

      old_name = gimp_object_get_name (GIMP_OBJECT (item));

      if (! old_name) old_name = "";
      if (! new_name) new_name = "";

      if (strcmp (old_name, new_name) &&
          gimp_item_rename (item, new_name, &error))
        {
          gimp_image_flush (gimp_item_get_image (item));
        }
      else
        {
          gchar *name = gimp_viewable_get_description (renderer->viewable, NULL);

          gtk_tree_store_set (GTK_TREE_STORE (tree_view->model), &iter,
                              GIMP_CONTAINER_TREE_VIEW_COLUMN_NAME, name,
                              -1);
          g_free (name);

          if (error)
            {
              gimp_message_literal (view->priv->image->gimp, G_OBJECT (view),
				    GIMP_MESSAGE_WARNING,
				    error->message);
              g_clear_error (&error);
            }
        }

      g_object_unref (renderer);
    }

  gtk_tree_path_free (path);
}


/*  "Visible" callbacks  */

static void
gimp_item_tree_view_visible_changed (GimpItem         *item,
                                     GimpItemTreeView *view)
{
  GimpContainerView     *container_view = GIMP_CONTAINER_VIEW (view);
  GimpContainerTreeView *tree_view      = GIMP_CONTAINER_TREE_VIEW (view);
  GtkTreeIter           *iter;

  iter = gimp_container_view_lookup (container_view,
                                     (GimpViewable *) item);

  if (iter)
    gtk_tree_store_set (GTK_TREE_STORE (tree_view->model), iter,
                        view->priv->model_column_visible,
                        gimp_item_get_visible (item),
                        -1);
}

static void
gimp_item_tree_view_eye_clicked (GtkCellRendererToggle *toggle,
                                 gchar                 *path_str,
                                 GdkModifierType        state,
                                 GimpItemTreeView      *view)
{
  gimp_item_tree_view_toggle_clicked (toggle, path_str, state, view,
                                      GIMP_UNDO_ITEM_VISIBILITY);
}

/*  "Linked" callbacks  */

static void
gimp_item_tree_view_linked_changed (GimpItem         *item,
                                    GimpItemTreeView *view)
{
  GimpContainerView     *container_view = GIMP_CONTAINER_VIEW (view);
  GimpContainerTreeView *tree_view      = GIMP_CONTAINER_TREE_VIEW (view);
  GtkTreeIter           *iter;

  iter = gimp_container_view_lookup (container_view,
                                     (GimpViewable *) item);

  if (iter)
    gtk_tree_store_set (GTK_TREE_STORE (tree_view->model), iter,
                        view->priv->model_column_linked,
                        gimp_item_get_linked (item),
                        -1);
}

static void
gimp_item_tree_view_chain_clicked (GtkCellRendererToggle *toggle,
                                   gchar                 *path_str,
                                   GdkModifierType        state,
                                   GimpItemTreeView      *view)
{
  gimp_item_tree_view_toggle_clicked (toggle, path_str, state, view,
                                      GIMP_UNDO_ITEM_LINKED);
}


/*  Utility functions used from eye_clicked and chain_clicked.
 *  Would make sense to do this in a generic fashion using
 *  properties, but for now it's better than duplicating the code.
 */
static void
gimp_item_tree_view_toggle_clicked (GtkCellRendererToggle *toggle,
                                    gchar                 *path_str,
                                    GdkModifierType        state,
                                    GimpItemTreeView      *view,
                                    GimpUndoType           undo_type)
{
  GimpContainerTreeView *tree_view = GIMP_CONTAINER_TREE_VIEW (view);
  GtkTreePath           *path;
  GtkTreeIter            iter;
  GimpUndoType           group_type;
  const gchar           *undo_desc;

  gboolean   (* getter) (const GimpItem *item);
  void       (* setter) (GimpItem       *item,
                         gboolean        value,
                         gboolean        push_undo);
  GimpUndo * (* pusher) (GimpImage      *image,
                         const gchar    *undo_desc,
                         GimpItem       *item);

  switch (undo_type)
    {
    case GIMP_UNDO_ITEM_VISIBILITY:
      getter     = gimp_item_get_visible;
      setter     = gimp_item_set_visible;
      pusher     = gimp_image_undo_push_item_visibility;
      group_type = GIMP_UNDO_GROUP_ITEM_VISIBILITY;
      undo_desc  = _("Set Item Exclusive Visible");
      break;

    case GIMP_UNDO_ITEM_LINKED:
      getter     = gimp_item_get_linked;
      setter     = gimp_item_set_linked;
      pusher     = gimp_image_undo_push_item_linked;
      group_type = GIMP_UNDO_GROUP_ITEM_LINKED;
      undo_desc  = _("Set Item Exclusive Linked");
      break;

    default:
      return;
    }

  path = gtk_tree_path_new_from_string (path_str);

  if (gtk_tree_model_get_iter (tree_view->model, &iter, path))
    {
      GimpContext      *context;
      GimpViewRenderer *renderer;
      GimpItem         *item;
      GimpImage        *image;
      gboolean          active;

      context = gimp_container_view_get_context (GIMP_CONTAINER_VIEW (view));

      gtk_tree_model_get (tree_view->model, &iter,
                          GIMP_CONTAINER_TREE_VIEW_COLUMN_RENDERER, &renderer,
                          -1);
      g_object_get (toggle,
                    "active", &active,
                    NULL);

      item = GIMP_ITEM (renderer->viewable);
      g_object_unref (renderer);

      image = gimp_item_get_image (item);

      if (state & GDK_SHIFT_MASK)
        {
          GList    *on  = NULL;
          GList    *off = NULL;
          GList    *list;
          gboolean  iter_valid;

          for (iter_valid = gtk_tree_model_get_iter_first (tree_view->model,
                                                           &iter);
               iter_valid;
               iter_valid = gtk_tree_model_iter_next (tree_view->model,
                                                      &iter))
            {
              gtk_tree_model_get (tree_view->model, &iter,
                                  GIMP_CONTAINER_TREE_VIEW_COLUMN_RENDERER, &renderer,
                                  -1);

              if ((GimpItem *) renderer->viewable != item)
                {
                  if (getter (GIMP_ITEM (renderer->viewable)))
                    on = g_list_prepend (on, renderer->viewable);
                  else
                    off = g_list_prepend (off, renderer->viewable);
                }

              g_object_unref (renderer);
            }

          if (on || off || ! getter (item))
            {
              GimpItemTreeViewClass *view_class;
              GimpUndo              *undo;
              gboolean               push_undo = TRUE;

              view_class = GIMP_ITEM_TREE_VIEW_GET_CLASS (view);

              undo = gimp_image_undo_can_compress (image, GIMP_TYPE_UNDO_STACK,
                                                   group_type);

              if (undo && (g_object_get_data (G_OBJECT (undo), "item-type") ==
                           (gpointer) view_class->item_type))
                push_undo = FALSE;

              if (push_undo)
                {
                  if (gimp_image_undo_group_start (image, group_type,
                                                   undo_desc))
                    {
                      undo = gimp_image_undo_can_compress (image,
                                                           GIMP_TYPE_UNDO_STACK,
                                                           group_type);

                      if (undo)
                        g_object_set_data (G_OBJECT (undo), "item-type",
                                           (gpointer) view_class->item_type);
                    }

                  pusher (image, NULL, item);

                  for (list = on; list; list = g_list_next (list))
                    pusher (image, NULL, list->data);

                  for (list = off; list; list = g_list_next (list))
                    pusher (image, NULL, list->data);

                  gimp_image_undo_group_end (image);
                }
              else
                {
                  gimp_undo_refresh_preview (undo, context);
                }
            }

          setter (item, TRUE, FALSE);

          if (on)
            {
              for (list = on; list; list = g_list_next (list))
                setter (GIMP_ITEM (list->data), FALSE, FALSE);
            }
          else if (off)
            {
              for (list = off; list; list = g_list_next (list))
                setter (GIMP_ITEM (list->data), TRUE, FALSE);
            }

          g_list_free (on);
          g_list_free (off);
        }
      else
        {
          GimpUndo *undo;
          gboolean  push_undo = TRUE;

          undo = gimp_image_undo_can_compress (image, GIMP_TYPE_ITEM_UNDO,
                                               undo_type);

          if (undo && GIMP_ITEM_UNDO (undo)->item == item)
            push_undo = FALSE;

          setter (item, ! active, push_undo);

          if (!push_undo)
            gimp_undo_refresh_preview (undo, context);
        }

      gimp_image_flush (image);
    }

  gtk_tree_path_free (path);
}
