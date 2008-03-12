/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-audio.h - Audio streaming object
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen <jorn@openedhand.com>
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
#ifndef _HAVE_CLUTTER_GST_AUDIO_H
#define _HAVE_CLUTTER_GST_AUDIO_H

#include <glib-object.h>
#include <clutter/clutter.h>
#include <gst/gstelement.h>

G_BEGIN_DECLS

#define CLUTTER_GST_TYPE_AUDIO clutter_gst_audio_get_type()

#define CLUTTER_GST_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_GST_TYPE_AUDIO, ClutterGstAudio))

#define CLUTTER_GST_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_GST_TYPE_AUDIO, ClutterGstAudioClass))

#define CLUTTER_GST_IS_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_GST_TYPE_AUDIO))

#define CLUTTER_GST_IS_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_GST_TYPE_AUDIO))

#define CLUTTER_GST_AUDIO_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_GST_TYPE_AUDIO, ClutterGstAudioClass))

typedef struct _ClutterGstAudio        ClutterGstAudio;
typedef struct _ClutterGstAudioClass   ClutterGstAudioClass;
typedef struct _ClutterGstAudioPrivate ClutterGstAudioPrivate;

/**
 * ClutterGstAudio:
 *
 * Simple class for playing audio files.
 *
 * The #ClutterGstAudio structure contains only private data and should
 * not be accessed directly.
 */
struct _ClutterGstAudio
{
  /*< private >*/
  GObject              parent;
  ClutterGstAudioPrivate *priv;
}; 

/**
 * ClutterGstAudioClass:
 *
 * Base class for #ClutterGstAudio.
 */
struct _ClutterGstAudioClass 
{
  /*< private >*/
  GObjectClass parent_class;

  /* Future padding */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
}; 

GType            clutter_gst_audio_get_type    (void) G_GNUC_CONST;
ClutterGstAudio *clutter_gst_audio_new         (void);

GstElement      *clutter_gst_audio_get_playbin (ClutterGstAudio *audio);

G_END_DECLS

#endif
