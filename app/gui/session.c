/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Session-managment stuff
 * Copyright (C) 1998 Sven Neumann <sven@gimp.org>
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

#include <errno.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#ifdef G_OS_WIN32
#include "libgimpbase/gimpwin32-io.h"
#endif

#include "gui-types.h"

#include "config/gimpconfig-file.h"
#include "config/gimpguiconfig.h"

#include "core/gimp.h"

#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpsessioninfo.h"

#include "dialogs/dialogs.h"

#include "session.h"
#include "gimp-log.h"

#include "gimp-intl.h"


enum
{
  SESSION_INFO = 1,
  LAST_TIP_SHOWN
};


static gchar * session_filename (Gimp *gimp);


/*  private variables  */

static gboolean   sessionrc_deleted = FALSE;


/*  public functions  */

void
session_init (Gimp *gimp)
{
  gchar      *filename;
  GScanner   *scanner;
  GTokenType  token;
  GError     *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  filename = session_filename (gimp);

  scanner = gimp_scanner_new_file (filename, &error);

  if (! scanner && error->code == GIMP_CONFIG_ERROR_OPEN_ENOENT)
    {
      g_clear_error (&error);
      g_free (filename);

      filename = g_build_filename (gimp_sysconf_directory (),
                                   "sessionrc", NULL);
      scanner = gimp_scanner_new_file (filename, NULL);
    }

  if (! scanner)
    {
      g_clear_error (&error);
      g_free (filename);
      return;
    }

  if (gimp->be_verbose)
    g_print ("Parsing '%s'\n", gimp_filename_to_utf8 (filename));

  g_scanner_scope_add_symbol (scanner, 0, "session-info",
                              GINT_TO_POINTER (SESSION_INFO));
  g_scanner_scope_add_symbol (scanner, 0,  "last-tip-shown",
                              GINT_TO_POINTER (LAST_TIP_SHOWN));

  token = G_TOKEN_LEFT_PAREN;

  while (g_scanner_peek_next_token (scanner) == token)
    {
      token = g_scanner_get_next_token (scanner);

      switch (token)
        {
        case G_TOKEN_LEFT_PAREN:
          token = G_TOKEN_SYMBOL;
          break;

        case G_TOKEN_SYMBOL:
          if (scanner->value.v_symbol == GINT_TO_POINTER (SESSION_INFO))
            {
              GimpDialogFactory *factory;
              GimpSessionInfo   *info;
              gchar             *factory_name;
              gchar             *entry_name;
              gboolean           skip = FALSE;

              token = G_TOKEN_STRING;

              if (! gimp_scanner_parse_string (scanner, &factory_name))
                break;

              factory = gimp_dialog_factory_from_name (factory_name);
              g_free (factory_name);

              if (! factory)
                break;

              if (! gimp_scanner_parse_string (scanner, &entry_name))
                break;

              info = gimp_session_info_new ();

              /* "dock" entries in the "dock" factory are just dummy
               * entries and don't have any dialog factory entry, so
               * don't bother looking for entires for them
               */
              if (!g_str_equal (entry_name, "dock"))
                {
                  GimpDialogFactoryEntry *entry =
                    gimp_dialog_factory_find_entry (factory,
                                                    entry_name);
                  if (entry)
                    {
                      gimp_session_info_set_factory_entry (info, entry);
                    }
                  else
                    {
                      /* If we expected a dialog factory entry but failed
                       * to find one, skip to add this session info object
                       */
                      skip = TRUE;
                    }
                }

              g_free (entry_name);

              if (gimp_config_deserialize (GIMP_CONFIG (info), scanner, 1, NULL))
                {
                  if (! skip)
                    {
                      GIMP_LOG (DIALOG_FACTORY,
                                "successfully parsed and added session info %p",
                                info);

                      factory->session_infos =
                        g_list_append (factory->session_infos, info);
                    }
                  else
                    {
                      GIMP_LOG (DIALOG_FACTORY,
                                "failed to parse session info %p, not adding",
                                info);

                      g_object_unref (info);
                    }
                }
              else
                {
                  g_object_unref (info);
                  break;
                }
            }
          else if (scanner->value.v_symbol == GINT_TO_POINTER (LAST_TIP_SHOWN))
            {
              GimpGuiConfig *config = GIMP_GUI_CONFIG (gimp->config);

              token = G_TOKEN_INT;

              if (! gimp_scanner_parse_int (scanner, &config->last_tip))
                break;
            }
          token = G_TOKEN_RIGHT_PAREN;
          break;

        case G_TOKEN_RIGHT_PAREN:
          token = G_TOKEN_LEFT_PAREN;
          break;

        default: /* do nothing */
          break;
        }
    }

  if (token != G_TOKEN_LEFT_PAREN)
    {
      g_scanner_get_next_token (scanner);
      g_scanner_unexp_token (scanner, token, NULL, NULL, NULL,
                             _("fatal parse error"), TRUE);
    }

  if (error)
    {
      gimp_message_literal (gimp, NULL, GIMP_MESSAGE_ERROR, error->message);
      g_clear_error (&error);

      gimp_config_file_backup_on_error (filename, "sessionrc", NULL);
    }

  gimp_scanner_destroy (scanner);
  g_free (filename);

  dialogs_load_recent_docks (gimp);
}

void
session_exit (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));
}

void
session_restore (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));

  gimp_dialog_factories_session_restore ();
}

void
session_save (Gimp     *gimp,
              gboolean  always_save)
{
  GimpConfigWriter *writer;
  gchar            *filename;
  GError           *error = NULL;

  g_return_if_fail (GIMP_IS_GIMP (gimp));

  if (sessionrc_deleted && ! always_save)
    return;

  filename = session_filename (gimp);

  if (gimp->be_verbose)
    g_print ("Writing '%s'\n", gimp_filename_to_utf8 (filename));

  writer =
    gimp_config_writer_new_file (filename,
                                 TRUE,
                                 "GIMP sessionrc\n\n"
                                 "This file takes session-specific info "
                                 "(that is info, you want to keep between "
                                 "two GIMP sessions).  You are not supposed "
                                 "to edit it manually, but of course you "
                                 "can do.  The sessionrc will be entirely "
                                 "rewritten every time you quit GIMP.  "
                                 "If this file isn't found, defaults are "
                                 "used.",
                                 NULL);
  g_free (filename);

  if (!writer)
    return;

  gimp_dialog_factories_session_save (writer);
  gimp_config_writer_linefeed (writer);

  /* save last tip shown
   *
   * FIXME: Make last-tip-shown increment only when used within the
   * session
   */
  gimp_config_writer_open (writer, "last-tip-shown");
  gimp_config_writer_printf (writer, "%d",
                             GIMP_GUI_CONFIG (gimp->config)->last_tip);
  gimp_config_writer_close (writer);

  if (! gimp_config_writer_finish (writer, "end of sessionrc", &error))
    {
      gimp_message_literal (gimp, NULL, GIMP_MESSAGE_ERROR, error->message);
      g_clear_error (&error);
    }

  dialogs_save_recent_docks (gimp);

  sessionrc_deleted = FALSE;
}

gboolean
session_clear (Gimp    *gimp,
               GError **error)
{
  gchar    *filename;
  gboolean  success = TRUE;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  filename = session_filename (gimp);

  if (g_unlink (filename) != 0 && errno != ENOENT)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
		   _("Deleting \"%s\" failed: %s"),
                   gimp_filename_to_utf8 (filename), g_strerror (errno));
      success = FALSE;
    }
  else
    {
      sessionrc_deleted = TRUE;
    }

  g_free (filename);

  return success;
}


static gchar *
session_filename (Gimp *gimp)
{
  gchar *filename = gimp_personal_rc_file ("sessionrc");

  if (gimp->session_name)
    {
      gchar *tmp = g_strconcat (filename, ".", gimp->session_name, NULL);

      g_free (filename);
      filename = tmp;
    }

  return filename;
}
