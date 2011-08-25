/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-player.h - Wrap some convenience functions around playbin2
 *
 * Authored By Damien Lespiau    <damien.lespiau@intel.com>
 *             Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *
 * Copyright (C) 2011 Intel Corporation
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

#ifndef __CLUTTER_GST_PLAYER_H__
#define __CLUTTER_GST_PLAYER_H__

#include <glib-object.h>

#include <clutter/clutter.h>

#include <clutter-gst/clutter-gst-types.h>

G_BEGIN_DECLS

#define CLUTTER_GST_TYPE_PLAYER clutter_gst_player_get_type()

#define CLUTTER_GST_PLAYER(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               CLUTTER_GST_TYPE_PLAYER,                 \
                               ClutterGstPlayer))
#define CLUTTER_GST_IS_PLAYER(obj)                              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                           \
                               CLUTTER_GST_TYPE_PLAYER))
#define CLUTTER_GST_PLAYER_GET_INTERFACE(obj)                           \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj),                                \
                                  CLUTTER_GST_TYPE_PLAYER,              \
                                  ClutterGstPlayerIface))

typedef struct _ClutterGstPlayer ClutterGstPlayer;
typedef struct _ClutterGstPlayerIface ClutterGstPlayerIface;
typedef struct _ClutterGstPlayerIfacePrivate ClutterGstPlayerIfacePrivate;


/**
 * ClutterGstPlayer
 *
 * #ClutterGstPlayer is an opaque structure whose members cannot be
 * directly accessed
 *
 * Since: 1.4
 */

/**
 * ClutterGstPlayerIface:
 * @download_buffering: handler for the #ClutterGstPlayer::download-buffering signal
 *
 * Interface vtable for #ClutterGstPlayer implementations
 *
 * Since: 1.4
 */
struct _ClutterGstPlayerIface
{
  /*< private >*/
  GTypeInterface base_iface;

  ClutterGstPlayerIfacePrivate *priv;

  /*< public >*/
  /* signals */
  void (* download_buffering) (ClutterGstPlayer *player,
			       gdouble           start,
			       gdouble           stop);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
};

GType clutter_gst_player_get_type (void) G_GNUC_CONST;

void                      clutter_gst_player_class_init          (GObjectClass *object_class);

gboolean                  clutter_gst_player_init                (ClutterGstPlayer        *player);

GstElement *		  clutter_gst_player_get_pipeline        (ClutterGstPlayer        *player);

gchar *			  clutter_gst_player_get_user_agent      (ClutterGstPlayer        *player);
void			  clutter_gst_player_set_user_agent      (ClutterGstPlayer        *player,
                                                                  const gchar             *user_agent);

ClutterGstSeekFlags	  clutter_gst_player_get_seek_flags      (ClutterGstPlayer        *player);
void			  clutter_gst_player_set_seek_flags      (ClutterGstPlayer        *player,
                                                                  ClutterGstSeekFlags      flags);

ClutterGstBufferingMode	  clutter_gst_player_get_buffering_mode  (ClutterGstPlayer        *player);
void			  clutter_gst_player_set_buffering_mode  (ClutterGstPlayer        *player,
                                                                  ClutterGstBufferingMode  mode);

GList *                   clutter_gst_player_get_audio_streams   (ClutterGstPlayer        *player);
gint                      clutter_gst_player_get_audio_stream    (ClutterGstPlayer        *player);
void                      clutter_gst_player_set_audio_stream    (ClutterGstPlayer        *player,
                                                                  gint                     index_);

gboolean                  clutter_gst_player_get_idle            (ClutterGstPlayer        *player);

G_END_DECLS

#endif /* __CLUTTER_GST_PLAYER_H__ */
