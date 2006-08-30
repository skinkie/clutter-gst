/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_CLUTTER_GST_MEDIA_H
#define _HAVE_CLUTTER_GST_MEDIA_H

#include <glib-object.h>
#include <gst/gsttaglist.h>

G_BEGIN_DECLS

#define CLUTTER_GST_TYPE_MEDIA clutter_gst_media_get_type()

#define CLUTTER_GST_MEDIA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_GST_TYPE_MEDIA, ClutterGstMedia))

#define CLUTTER_GST_IS_MEDIA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_GST_TYPE_MEDIA))

#define CLUTTER_GST_MEDIA_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  CLUTTER_GST_TYPE_MEDIA, ClutterGstMediaInterface))

typedef struct _ClutterGstMedia           ClutterGstMedia;      
typedef struct _ClutterGstMediaInterface  ClutterGstMediaInterface;

struct _ClutterGstMediaInterface
{
  GTypeInterface	    base_iface;
  void (*set_uri)           (ClutterGstMedia *media,
			     const char      *uri);
  const char *(*get_uri)    (ClutterGstMedia *media);
  void (*set_playing)       (ClutterGstMedia *media,
			     gboolean         playing);
  gboolean (*get_playing)   (ClutterGstMedia *media);
  void (*set_position)      (ClutterGstMedia *media,
			     int              position);
  int (*get_position)       (ClutterGstMedia *media);
  void (*set_volume)        (ClutterGstMedia *media,
			     double           volume);
  double (*get_volume)      (ClutterGstMedia *media);
  gboolean (*can_seek)      (ClutterGstMedia *media);
  int (*get_buffer_percent) (ClutterGstMedia *media);
  int (*get_duration)       (ClutterGstMedia *media);

  /* signals */

  void (* metadata_available) (ClutterGstMedia *media,
			       GstTagList      *tag_list);
  void (* eos)                (ClutterGstMedia *media);
  void (* error)              (ClutterGstMedia *media,
			       GError          *error);
};


GType clutter_gst_media_get_type     (void);

void
clutter_gst_media_set_uri            (ClutterGstMedia *media,
				      const char      *uri);
const char *
clutter_gst_media_get_uri            (ClutterGstMedia *media);

void
clutter_gst_media_set_playing        (ClutterGstMedia *media,
				      gboolean         playing);

gboolean
clutter_gst_media_get_playing        (ClutterGstMedia *media);

void
clutter_gst_media_set_position       (ClutterGstMedia *media,
				      int              position);

int
clutter_gst_media_get_position       (ClutterGstMedia *media);

void
clutter_gst_media_set_volume         (ClutterGstMedia *media,
				      double           volume);

double
clutter_gst_media_get_volume         (ClutterGstMedia *media);

gboolean
clutter_gst_media_get_can_seek       (ClutterGstMedia *media);

int
clutter_gst_media_get_buffer_percent (ClutterGstMedia *media);

int
clutter_gst_media_get_duration       (ClutterGstMedia *media);

void
clutter_gst_media_set_filename       (ClutterGstMedia *media, 
				      const gchar     *filename);

G_END_DECLS

#endif
