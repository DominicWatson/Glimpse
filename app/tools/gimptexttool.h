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

#ifndef __GIMP_TEXT_TOOL_H__
#define __GIMP_TEXT_TOOL_H__


#define TEXT_TOOL_HACK 1


#include "gimpdrawtool.h"


#define GIMP_TYPE_TEXT_TOOL            (gimp_text_tool_get_type ())
#define GIMP_TEXT_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_TEXT_TOOL, GimpTextTool))
#define GIMP_IS_TEXT_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_TEXT_TOOL))
#define GIMP_TEXT_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_TEXT_TOOL, GimpTextToolClass))
#define GIMP_IS_TEXT_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_TEXT_TOOL))

#define GIMP_TEXT_TOOL_GET_OPTIONS(t)  (GIMP_TEXT_OPTIONS (gimp_tool_get_options (GIMP_TOOL (t))))


typedef struct _GimpTextTool       GimpTextTool;
typedef struct _GimpTextToolClass  GimpTextToolClass;

struct _GimpTextTool
{
  GimpDrawTool    parent_instance;

  GimpText       *proxy;
  GList          *pending;
  guint           idle_id;

  gboolean        moving;

  GtkTextBuffer  *text_buffer;

  GimpText       *text;
  GimpTextLayer  *layer;
  GimpImage      *image;

  GtkWidget      *editor;
  GtkWidget      *confirm_dialog;
  GimpUIManager  *ui_manager;
  GtkIMContext   *im_context;

  gboolean        needs_im_reset;

  gchar          *preedit_string;
  gint            preedit_len;
  gint            preedit_cursor;

  gboolean        handle_rectangle_change_complete;
  gboolean        text_box_fixed;

  gboolean        selecting;
  gint            select_start_offset;
  gboolean        select_words;
  gboolean        select_lines;

  gint            x_pos;

  GimpTextLayout *layout;

#ifdef TEXT_TOOL_HACK
  GtkWidget      *proxy_text_view; /* this sucks so much */
#endif
};

struct _GimpTextToolClass
{
  GimpDrawToolClass parent_class;

#ifndef TEXT_TOOL_HACK
  void (* move_cursor)        (GimpTextTool    *text_tool,
                               GtkMovementStep  step,
                               gint             count,
                               gboolean         extend_selection);

  void (* delete_from_cursor) (GimpTextTool    *text_tool,
                               GtkDeleteType    type,
                               gint             count);

  void (* backspace)          (GimpTextTool    *text_tool);
#endif
};


void       gimp_text_tool_register               (GimpToolRegisterCallback  callback,
                                                  gpointer                  data);

GType      gimp_text_tool_get_type               (void) G_GNUC_CONST;

void       gimp_text_tool_set_layer              (GimpTextTool *text_tool,
                                                  GimpLayer    *layer);

gboolean   gimp_text_tool_get_has_text_selection (GimpTextTool *text_tool);

void       gimp_text_tool_delete_text            (GimpTextTool *text_tool,
                                                  gboolean      backspace);
void       gimp_text_tool_clipboard_cut          (GimpTextTool *text_tool);
void       gimp_text_tool_clipboard_copy         (GimpTextTool *text_tool,
                                                  gboolean      use_clipboard);
void       gimp_text_tool_clipboard_paste        (GimpTextTool *text_tool,
                                                  gboolean      use_clipboard);

void       gimp_text_tool_create_vectors         (GimpTextTool *text_tool);
void       gimp_text_tool_create_vectors_warped  (GimpTextTool *text_tool);

#endif /* __GIMP_TEXT_TOOL_H__ */
