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

#ifndef _HAVE_CLUTTER_GST_VIDEO_TEXTURE_H
#define _HAVE_CLUTTER_GST_VIDEO_TEXTURE_H

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-texture.h>
#include <clutter-gst/clutter-gst-media.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define CLUTTER_GST_TYPE_VIDEO_TEXTURE clutter_gst_video_texture_get_type()

#define CLUTTER_GST_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_GST_TYPE_VIDEO_TEXTURE, ClutterGstVideoTexture))

#define CLUTTER_GST_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_GST_TYPE_VIDEO_TEXTURE, ClutterGstVideoTextureClass))

#define CLUTTER_GST_IS_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_GST_TYPE_VIDEO_TEXTURE))

#define CLUTTER_GST_IS_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_GST_TYPE_VIDEO_TEXTURE))

#define CLUTTER_GST_VIDEO_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_GST_TYPE_VIDEO_TEXTURE, ClutterGstVideoTextureClass))

typedef struct _ClutterGstVideoTexture        ClutterGstVideoTexture;
typedef struct _ClutterGstVideoTextureClass   ClutterGstVideoTextureClass;
typedef struct _ClutterGstVideoTexturePrivate ClutterGstVideoTexturePrivate;

struct _ClutterGstVideoTexture
{
  ClutterTexture              parent;
  ClutterGstVideoTexturePrivate *priv;
}; 

struct _ClutterGstVideoTextureClass 
{
  ClutterTextureClass parent_class;

  /* Future padding */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
}; 

GType         clutter_gst_video_texture_get_type (void) G_GNUC_CONST;
ClutterActor *clutter_gst_video_texture_new      (void);

G_END_DECLS

#endif
