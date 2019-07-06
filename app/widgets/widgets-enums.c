
/* Generated data (by gimp-mkenums) */

#include "config.h"
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "widgets-enums.h"
#include "gimp-intl.h"

/* enumerations from "./widgets-enums.h" */
GType
gimp_active_color_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_ACTIVE_COLOR_FOREGROUND, "GIMP_ACTIVE_COLOR_FOREGROUND", "foreground" },
    { GIMP_ACTIVE_COLOR_BACKGROUND, "GIMP_ACTIVE_COLOR_BACKGROUND", "background" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_ACTIVE_COLOR_FOREGROUND, NC_("active-color", "Foreground"), NULL },
    { GIMP_ACTIVE_COLOR_BACKGROUND, NC_("active-color", "Background"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpActiveColor", values);
      gimp_type_set_translation_context (type, "active-color");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_dialog_state_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_DIALOG_OK, "GIMP_COLOR_DIALOG_OK", "ok" },
    { GIMP_COLOR_DIALOG_CANCEL, "GIMP_COLOR_DIALOG_CANCEL", "cancel" },
    { GIMP_COLOR_DIALOG_UPDATE, "GIMP_COLOR_DIALOG_UPDATE", "update" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_DIALOG_OK, "GIMP_COLOR_DIALOG_OK", NULL },
    { GIMP_COLOR_DIALOG_CANCEL, "GIMP_COLOR_DIALOG_CANCEL", NULL },
    { GIMP_COLOR_DIALOG_UPDATE, "GIMP_COLOR_DIALOG_UPDATE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpColorDialogState", values);
      gimp_type_set_translation_context (type, "color-dialog-state");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_frame_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_FRAME_MODE_PIXEL, "GIMP_COLOR_FRAME_MODE_PIXEL", "pixel" },
    { GIMP_COLOR_FRAME_MODE_RGB, "GIMP_COLOR_FRAME_MODE_RGB", "rgb" },
    { GIMP_COLOR_FRAME_MODE_HSV, "GIMP_COLOR_FRAME_MODE_HSV", "hsv" },
    { GIMP_COLOR_FRAME_MODE_CMYK, "GIMP_COLOR_FRAME_MODE_CMYK", "cmyk" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_FRAME_MODE_PIXEL, NC_("color-frame-mode", "Pixel"), NULL },
    { GIMP_COLOR_FRAME_MODE_RGB, NC_("color-frame-mode", "RGB"), NULL },
    { GIMP_COLOR_FRAME_MODE_HSV, NC_("color-frame-mode", "HSV"), NULL },
    { GIMP_COLOR_FRAME_MODE_CMYK, NC_("color-frame-mode", "CMYK"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpColorFrameMode", values);
      gimp_type_set_translation_context (type, "color-frame-mode");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_pick_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_PICK_MODE_NONE, "GIMP_COLOR_PICK_MODE_NONE", "none" },
    { GIMP_COLOR_PICK_MODE_FOREGROUND, "GIMP_COLOR_PICK_MODE_FOREGROUND", "foreground" },
    { GIMP_COLOR_PICK_MODE_BACKGROUND, "GIMP_COLOR_PICK_MODE_BACKGROUND", "background" },
    { GIMP_COLOR_PICK_MODE_PALETTE, "GIMP_COLOR_PICK_MODE_PALETTE", "palette" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_PICK_MODE_NONE, NC_("color-pick-mode", "Pick only"), NULL },
    { GIMP_COLOR_PICK_MODE_FOREGROUND, NC_("color-pick-mode", "Set foreground color"), NULL },
    { GIMP_COLOR_PICK_MODE_BACKGROUND, NC_("color-pick-mode", "Set background color"), NULL },
    { GIMP_COLOR_PICK_MODE_PALETTE, NC_("color-pick-mode", "Add to palette"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpColorPickMode", values);
      gimp_type_set_translation_context (type, "color-pick-mode");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_color_pick_state_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_COLOR_PICK_STATE_NEW, "GIMP_COLOR_PICK_STATE_NEW", "new" },
    { GIMP_COLOR_PICK_STATE_UPDATE, "GIMP_COLOR_PICK_STATE_UPDATE", "update" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_COLOR_PICK_STATE_NEW, "GIMP_COLOR_PICK_STATE_NEW", NULL },
    { GIMP_COLOR_PICK_STATE_UPDATE, "GIMP_COLOR_PICK_STATE_UPDATE", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpColorPickState", values);
      gimp_type_set_translation_context (type, "color-pick-state");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_cursor_format_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_CURSOR_FORMAT_BITMAP, "GIMP_CURSOR_FORMAT_BITMAP", "bitmap" },
    { GIMP_CURSOR_FORMAT_PIXBUF, "GIMP_CURSOR_FORMAT_PIXBUF", "pixbuf" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_CURSOR_FORMAT_BITMAP, NC_("cursor-format", "Black & white"), NULL },
    { GIMP_CURSOR_FORMAT_PIXBUF, NC_("cursor-format", "Fancy"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpCursorFormat", values);
      gimp_type_set_translation_context (type, "cursor-format");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_help_browser_type_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_HELP_BROWSER_GIMP, "GIMP_HELP_BROWSER_GIMP", "gimp" },
    { GIMP_HELP_BROWSER_WEB_BROWSER, "GIMP_HELP_BROWSER_WEB_BROWSER", "web-browser" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_HELP_BROWSER_GIMP, NC_("help-browser-type", "GIMP help browser"), NULL },
    { GIMP_HELP_BROWSER_WEB_BROWSER, NC_("help-browser-type", "Web browser"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpHelpBrowserType", values);
      gimp_type_set_translation_context (type, "help-browser-type");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_histogram_scale_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_HISTOGRAM_SCALE_LINEAR, "GIMP_HISTOGRAM_SCALE_LINEAR", "linear" },
    { GIMP_HISTOGRAM_SCALE_LOGARITHMIC, "GIMP_HISTOGRAM_SCALE_LOGARITHMIC", "logarithmic" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_HISTOGRAM_SCALE_LINEAR, NC_("histogram-scale", "Linear histogram"), NULL },
    { GIMP_HISTOGRAM_SCALE_LOGARITHMIC, NC_("histogram-scale", "Logarithmic histogram"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpHistogramScale", values);
      gimp_type_set_translation_context (type, "histogram-scale");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_tab_style_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TAB_STYLE_ICON, "GIMP_TAB_STYLE_ICON", "icon" },
    { GIMP_TAB_STYLE_PREVIEW, "GIMP_TAB_STYLE_PREVIEW", "preview" },
    { GIMP_TAB_STYLE_NAME, "GIMP_TAB_STYLE_NAME", "name" },
    { GIMP_TAB_STYLE_BLURB, "GIMP_TAB_STYLE_BLURB", "blurb" },
    { GIMP_TAB_STYLE_ICON_NAME, "GIMP_TAB_STYLE_ICON_NAME", "icon-name" },
    { GIMP_TAB_STYLE_ICON_BLURB, "GIMP_TAB_STYLE_ICON_BLURB", "icon-blurb" },
    { GIMP_TAB_STYLE_PREVIEW_NAME, "GIMP_TAB_STYLE_PREVIEW_NAME", "preview-name" },
    { GIMP_TAB_STYLE_PREVIEW_BLURB, "GIMP_TAB_STYLE_PREVIEW_BLURB", "preview-blurb" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TAB_STYLE_ICON, NC_("tab-style", "Icon"), NULL },
    { GIMP_TAB_STYLE_PREVIEW, NC_("tab-style", "Current status"), NULL },
    { GIMP_TAB_STYLE_NAME, NC_("tab-style", "Text"), NULL },
    { GIMP_TAB_STYLE_BLURB, NC_("tab-style", "Description"), NULL },
    { GIMP_TAB_STYLE_ICON_NAME, NC_("tab-style", "Icon & text"), NULL },
    { GIMP_TAB_STYLE_ICON_BLURB, NC_("tab-style", "Icon & desc"), NULL },
    { GIMP_TAB_STYLE_PREVIEW_NAME, NC_("tab-style", "Status & text"), NULL },
    { GIMP_TAB_STYLE_PREVIEW_BLURB, NC_("tab-style", "Status & desc"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpTabStyle", values);
      gimp_type_set_translation_context (type, "tab-style");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_tag_entry_mode_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_TAG_ENTRY_MODE_QUERY, "GIMP_TAG_ENTRY_MODE_QUERY", "query" },
    { GIMP_TAG_ENTRY_MODE_ASSIGN, "GIMP_TAG_ENTRY_MODE_ASSIGN", "assign" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_TAG_ENTRY_MODE_QUERY, "GIMP_TAG_ENTRY_MODE_QUERY", NULL },
    { GIMP_TAG_ENTRY_MODE_ASSIGN, "GIMP_TAG_ENTRY_MODE_ASSIGN", NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpTagEntryMode", values);
      gimp_type_set_translation_context (type, "tag-entry-mode");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}

GType
gimp_window_hint_get_type (void)
{
  static const GEnumValue values[] =
  {
    { GIMP_WINDOW_HINT_NORMAL, "GIMP_WINDOW_HINT_NORMAL", "normal" },
    { GIMP_WINDOW_HINT_UTILITY, "GIMP_WINDOW_HINT_UTILITY", "utility" },
    { GIMP_WINDOW_HINT_KEEP_ABOVE, "GIMP_WINDOW_HINT_KEEP_ABOVE", "keep-above" },
    { 0, NULL, NULL }
  };

  static const GimpEnumDesc descs[] =
  {
    { GIMP_WINDOW_HINT_NORMAL, NC_("window-hint", "Normal window"), NULL },
    { GIMP_WINDOW_HINT_UTILITY, NC_("window-hint", "Utility window"), NULL },
    { GIMP_WINDOW_HINT_KEEP_ABOVE, NC_("window-hint", "Keep above"), NULL },
    { 0, NULL, NULL }
  };

  static GType type = 0;

  if (G_UNLIKELY (! type))
    {
      type = g_enum_register_static ("GimpWindowHint", values);
      gimp_type_set_translation_context (type, "window-hint");
      gimp_enum_set_value_descriptions (type, descs);
    }

  return type;
}


/* Generated data ends here */

