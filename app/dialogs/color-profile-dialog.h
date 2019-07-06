/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * color-profile-dialog.h
 * Copyright (C) 2015 Michael Natterer <mitch@gimp.org>
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

#ifndef __COLOR_PROFILE_DIALOG_H__
#define __COLOR_PROFILE_DIALOG_H__


GtkWidget * color_profile_assign_dialog_new  (GimpImage    *image,
                                              GimpContext  *context,
                                              GtkWidget    *parent,
                                              GimpProgress *progress);

GtkWidget * color_profile_convert_dialog_new (GimpImage    *image,
                                              GimpContext  *context,
                                              GtkWidget    *parent,
                                              GimpProgress *progress);


#endif  /*  __COLOR_PROFILE_DIALOG_H__  */
