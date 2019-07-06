/* GIMP - The GNU Image Manipulation Program
 *
 * gimpwarptool.c
 * Copyright (C) 2011 Michael Muré <batolettre@gmail.com>
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
#include <stdlib.h>

#include <gegl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpdrawable-shadow.h"
#include "core/gimpimage.h"
#include "core/gimpimagemap.h"
#include "core/gimplayer.h"
#include "core/gimpprogress.h"
#include "core/gimpprojection.h"

#include "widgets/gimphelp-ids.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-transform.h"

#include "gimpwarptool.h"
#include "gimpwarpoptions.h"
#include "gimptoolcontrol.h"

#include "gimp-intl.h"


static void       gimp_warp_tool_start              (GimpWarpTool          *wt,
                                                     GimpDisplay           *display);

static void       gimp_warp_tool_options_notify     (GimpTool              *tool,
                                                     GimpToolOptions       *options,
                                                     const GParamSpec      *pspec);
static void       gimp_warp_tool_button_press       (GimpTool              *tool,
                                                     const GimpCoords      *coords,
                                                     guint32                time,
                                                     GdkModifierType        state,
                                                     GimpButtonPressType    press_type,
                                                     GimpDisplay           *display);
static void       gimp_warp_tool_button_release     (GimpTool              *tool,
                                                     const GimpCoords      *coords,
                                                     guint32                time,
                                                     GdkModifierType        state,
                                                     GimpButtonReleaseType  release_type,
                                                     GimpDisplay           *display);
static gboolean   gimp_warp_tool_key_press          (GimpTool              *tool,
                                                     GdkEventKey           *kevent,
                                                     GimpDisplay           *display);
static void       gimp_warp_tool_motion             (GimpTool              *tool,
                                                     const GimpCoords      *coords,
                                                     guint32                time,
                                                     GdkModifierType        state,
                                                     GimpDisplay           *display);
static void       gimp_warp_tool_control            (GimpTool              *tool,
                                                     GimpToolAction         action,
                                                     GimpDisplay           *display);
static void       gimp_warp_tool_cursor_update      (GimpTool              *tool,
                                                     const GimpCoords      *coords,
                                                     GdkModifierType        state,
                                                     GimpDisplay           *display);
static void       gimp_warp_tool_oper_update        (GimpTool              *tool,
                                                     const GimpCoords      *coords,
                                                     GdkModifierType        state,
                                                     gboolean               proximity,
                                                     GimpDisplay           *display);

static void       gimp_warp_tool_draw               (GimpDrawTool          *draw_tool);

G_DEFINE_TYPE (GimpWarpTool, gimp_warp_tool, GIMP_TYPE_DRAW_TOOL)

#define parent_class gimp_warp_tool_parent_class


void
gimp_warp_tool_register (GimpToolRegisterCallback  callback,
                         gpointer                  data)
{
  (* callback) (GIMP_TYPE_WARP_TOOL,
                GIMP_TYPE_WARP_OPTIONS,
                gimp_warp_options_gui,
                0,
                "gimp-warp-tool",
                _("Warp Transform"),
                _("Warp Transform: Deform with different tools"),
                N_("_Warp Transform"), "W",
                NULL, GIMP_HELP_TOOL_WARP,
                GIMP_STOCK_TOOL_WARP,
                data);
}

static void
gimp_warp_tool_class_init (GimpWarpToolClass *klass)
{
  GimpToolClass     *tool_class      = GIMP_TOOL_CLASS (klass);
  GimpDrawToolClass *draw_tool_class = GIMP_DRAW_TOOL_CLASS (klass);

  tool_class->options_notify = gimp_warp_tool_options_notify;
  tool_class->button_press   = gimp_warp_tool_button_press;
  tool_class->button_release = gimp_warp_tool_button_release;
  tool_class->key_press      = gimp_warp_tool_key_press;
  tool_class->motion         = gimp_warp_tool_motion;
  tool_class->control        = gimp_warp_tool_control;
  tool_class->cursor_update  = gimp_warp_tool_cursor_update;
  tool_class->oper_update    = gimp_warp_tool_oper_update;

  draw_tool_class->draw      = gimp_warp_tool_draw;
}

static void
gimp_warp_tool_init (GimpWarpTool *self)
{
  GimpTool *tool = GIMP_TOOL (self);

  gimp_tool_control_set_dirty_mask  (tool->control,
                                     GIMP_DIRTY_IMAGE           |
                                     GIMP_DIRTY_IMAGE_STRUCTURE |
                                     GIMP_DIRTY_DRAWABLE        |
                                     GIMP_DIRTY_SELECTION);
  gimp_tool_control_set_wants_click (tool->control, TRUE);
  gimp_tool_control_set_tool_cursor (tool->control,
                                     GIMP_TOOL_CURSOR_PERSPECTIVE);
}

static void
gimp_warp_tool_control (GimpTool       *tool,
                        GimpToolAction  action,
                        GimpDisplay    *display)
{
  GimpWarpTool *wt = GIMP_WARP_TOOL (tool);

  switch (action)
    {
    case GIMP_TOOL_ACTION_PAUSE:
    case GIMP_TOOL_ACTION_RESUME:
    case GIMP_TOOL_ACTION_HALT:
      break;
    }

  GIMP_TOOL_CLASS (parent_class)->control (tool, action, display);
}

static void
gimp_warp_tool_start (GimpWarpTool *wt,
                      GimpDisplay  *display)
{
  GimpTool     *tool     = GIMP_TOOL (wt);

  gimp_tool_control (tool, GIMP_TOOL_ACTION_HALT, display);

  tool->display = display;

  gimp_draw_tool_start (GIMP_DRAW_TOOL (wt), display);
}

static void
gimp_warp_tool_options_notify (GimpTool         *tool,
                               GimpToolOptions  *options,
                               const GParamSpec *pspec)
{
  GimpWarpTool *ct = GIMP_WARP_TOOL (tool);

  GIMP_TOOL_CLASS (parent_class)->options_notify (tool, options, pspec);

  if (! tool->display)
    return;

  gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

  if (strcmp (pspec->name, "foo") == 0)
    {
    }

  gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
}

static gboolean
gimp_warp_tool_key_press (GimpTool    *tool,
                          GdkEventKey *kevent,
                          GimpDisplay *display)
{
  GimpWarpTool *wt = GIMP_WARP_TOOL (tool);

  switch (kevent->keyval)
    {
    case GDK_KEY_BackSpace:
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter:
      return TRUE;

    case GDK_KEY_Escape:
      gimp_tool_control (tool, GIMP_TOOL_ACTION_HALT, display);
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static void
gimp_warp_tool_motion (GimpTool         *tool,
                       const GimpCoords *coords,
                       guint32           time,
                       GdkModifierType   state,
                       GimpDisplay      *display)
{
  GimpWarpTool    *wt       = GIMP_WARP_TOOL (tool);

  gimp_draw_tool_pause (GIMP_DRAW_TOOL (tool));

  gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
}

static void
gimp_warp_tool_oper_update (GimpTool         *tool,
                            const GimpCoords *coords,
                            GdkModifierType   state,
                            gboolean          proximity,
                            GimpDisplay      *display)
{
  GimpWarpTool *wt        = GIMP_WARP_TOOL (tool);
  GimpDrawTool *draw_tool = GIMP_DRAW_TOOL (tool);

  gimp_draw_tool_pause (draw_tool);

  gimp_draw_tool_resume (draw_tool);
}

static void
gimp_warp_tool_button_press (GimpTool            *tool,
                             const GimpCoords    *coords,
                             guint32              time,
                             GdkModifierType      state,
                             GimpButtonPressType  press_type,
                             GimpDisplay         *display)
{
  GimpWarpTool    *wt        = GIMP_WARP_TOOL (tool);
  GimpDrawTool    *draw_tool = GIMP_DRAW_TOOL (tool);

  if (display != tool->display)
    gimp_warp_tool_start (wt, display);

  gimp_tool_control_activate (tool->control);
}

void
gimp_warp_tool_button_release (GimpTool              *tool,
                               const GimpCoords      *coords,
                               guint32                time,
                               GdkModifierType        state,
                               GimpButtonReleaseType  release_type,
                               GimpDisplay           *display)
{
  GimpWarpTool    *wt      = GIMP_WARP_TOOL (tool);

  gimp_draw_tool_pause (GIMP_DRAW_TOOL (wt));

  gimp_tool_control_halt (tool->control);

  if (release_type == GIMP_BUTTON_RELEASE_CANCEL)
    {
    }
  else
    {
    }

  gimp_draw_tool_resume (GIMP_DRAW_TOOL (tool));
}

static void
gimp_warp_tool_cursor_update (GimpTool         *tool,
                              const GimpCoords *coords,
                              GdkModifierType   state,
                              GimpDisplay      *display)
{
  GimpCursorModifier  modifier = GIMP_CURSOR_MODIFIER_PLUS;

  if (tool->display)
    {
      modifier = GIMP_CURSOR_MODIFIER_MOVE;
    }

  gimp_tool_control_set_cursor_modifier (tool->control, modifier);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, display);
}

static void
gimp_warp_tool_draw (GimpDrawTool *draw_tool)
{
  GimpWarpTool    *wt        = GIMP_WARP_TOOL (draw_tool);
}
