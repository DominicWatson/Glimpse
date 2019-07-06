/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis
 *
 * gimpfilteredcontainer.h
 * Copyright (C) 2008 Aurimas Juška <aurisj@svn.gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GIMP_FILTERED_CONTAINER_H__
#define __GIMP_FILTERED_CONTAINER_H__


#include "gimplist.h"


#define GIMP_TYPE_FILTERED_CONTAINER            (gimp_filtered_container_get_type ())
#define GIMP_FILTERED_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_FILTERED_CONTAINER, GimpFilteredContainer))
#define GIMP_FILTERED_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_FILTERED_CONTAINER, GimpFilteredContainerClass))
#define GIMP_IS_FILTERED_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_FILTERED_CONTAINER))
#define GIMP_FILTERED_CONTAINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_FILTERED_CONTAINER, GimpFilteredContainerClass))


typedef struct _GimpFilteredContainerClass GimpFilteredContainerClass;

struct _GimpFilteredContainer
{
  GimpList              parent_instance;

  GimpContainer        *src_container;
  GList                *filter;
  GHashTable           *tag_ref_counts;
  gint                  tag_count;
};

struct _GimpFilteredContainerClass
{
  GimpContainerClass  parent_class;

  void      (* tag_count_changed)     (GimpFilteredContainer  *container,
                                       gint                    count);
};


GType           gimp_filtered_container_get_type      (void) G_GNUC_CONST;

GimpContainer * gimp_filtered_container_new           (GimpContainer           *src_container,
                                                       GCompareFunc             sort_func);

void            gimp_filtered_container_set_filter    (GimpFilteredContainer   *filtered_container,
                                                       GList                   *tags);

const GList   * gimp_filtered_container_get_filter    (GimpFilteredContainer   *filtered_container);

gint            gimp_filtered_container_get_tag_count (GimpFilteredContainer   *container);

#endif  /* __GIMP_FILTERED_CONTAINER_H__ */
