/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-video-texture.c - ClutterTexture using GStreamer to display a
 *                               video stream.
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

/**
 * SECTION:clutter-gst-video-texture
 * @short_description: Actor for playback of video files.
 *
 * #ClutterGstVideoTexture is a #ClutterTexture that plays video files.
 */

#include "config.h"

#include "clutter-gst-video-texture.h"
#include "clutter-gst-video-sink.h"

#include <gst/gst.h>
#include <glib.h>

struct _ClutterGstVideoTexturePrivate
{
  GstElement *playbin;

  gchar *uri;

  guint can_seek : 1;

  guint tick_timeout_id;

  gdouble buffer_fill;
  guint duration;
};

enum {
  PROP_0,

  /* ClutterMedia proprs */
  PROP_URI,
  PROP_PLAYING,
  PROP_PROGRESS,
  PROP_AUDIO_VOLUME,
  PROP_CAN_SEEK,
  PROP_BUFFER_FILL,
  PROP_DURATION
};


#define TICK_TIMEOUT 0.5

static void clutter_media_init (ClutterMediaIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterGstVideoTexture,
                         clutter_gst_video_texture,
                         CLUTTER_TYPE_TEXTURE,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

/* Interface implementation */

static gboolean
tick_timeout (gpointer data)
{
  GObject *video_texture = data;

  g_object_notify (video_texture, "progress");

  return TRUE;
}

static void
set_uri (ClutterGstVideoTexture *video_texture,
         const gchar            *uri)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GObject *self = G_OBJECT (video_texture);
  GstState state, pending;

  if (!priv->playbin)
    return;

  g_free (priv->uri);

  if (uri) 
    {
      priv->uri = g_strdup (uri);

      /* Ensure the tick timeout is installed.
       * 
       * We also have it installed in PAUSED state, because
       * seeks etc may have a delayed effect on the position.
       */
      if (priv->tick_timeout_id == 0)
        {
          priv->tick_timeout_id =
            g_timeout_add (TICK_TIMEOUT * 1000, tick_timeout, self);
        }
    }
  else 
    {
      priv->uri = NULL;

      if (priv->tick_timeout_id != 0)
	{
	  g_source_remove (priv->tick_timeout_id);
	  priv->tick_timeout_id = 0;
	}
    }

  priv->can_seek = FALSE;
  priv->duration = 0;

  gst_element_get_state (priv->playbin, &state, &pending, 0);

  if (pending)
    state = pending;

  gst_element_set_state (priv->playbin, GST_STATE_NULL);

  g_object_set (priv->playbin, "uri", uri, NULL);

  /*
   * Restore state.
   */
  if (uri)
    gst_element_set_state (priv->playbin, state);

  /*
   * Emit notififications for all these to make sure UI is not showing
   * any properties of the old URI.
   */
  g_object_notify (self, "uri");
  g_object_notify (self, "can-seek");
  g_object_notify (self, "duration");
  g_object_notify (self, "progress");
}

static void
set_playing (ClutterGstVideoTexture *video_texture,
             gboolean                playing)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->playbin)
    return;

  if (priv->uri) 
    {
      GstState state = GST_STATE_PAUSED;

      if (playing)
	state = GST_STATE_PLAYING;

      gst_element_set_state (priv->playbin, state);
    } 
  else 
    {
      if (playing)
	g_warning ("Unable to start playing: no URI is set");
    }
  
  g_object_notify (G_OBJECT (video_texture), "playing");
  g_object_notify (G_OBJECT (video_texture), "progress");
}

static gboolean
get_playing (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstState state, pending;

  if (!priv->playbin)
    return FALSE;
  
  gst_element_get_state (priv->playbin, &state, &pending, 0);
  
  if (pending)
    return (pending == GST_STATE_PLAYING);
  else
    return (state == GST_STATE_PLAYING);
}

static void
set_progress (ClutterGstVideoTexture *video_texture,
              gdouble                 progress)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstState state, pending;
  GstQuery *duration_q;
  gint64 position;

  if (!priv->playbin)
    return;

  gst_element_get_state (priv->playbin, &state, &pending, 0);

  if (pending)
    state = pending;

  gst_element_set_state (priv->playbin, GST_STATE_PAUSED);

  duration_q = gst_query_new_duration (GST_FORMAT_TIME);

  if (gst_element_query (priv->playbin, duration_q))
    {
      gint64 duration = 0;

      gst_query_parse_duration (duration_q, NULL, &duration);

      position = progress * duration;
    }
  else
    position = 0;

  gst_query_unref (duration_q);

  gst_element_seek (priv->playbin,
		    1.0,
		    GST_FORMAT_TIME,
		    GST_SEEK_FLAG_FLUSH,
		    GST_SEEK_TYPE_SET,
		    position,
		    0, 0);

  gst_element_set_state (priv->playbin, state);

  g_object_notify (G_OBJECT (video_texture), "progress");
}

static gdouble
get_progress (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstQuery *position_q, *duration_q;
  gdouble progress;

  if (!priv->playbin)
    return 0.0;

  position_q = gst_query_new_position (GST_FORMAT_TIME);
  duration_q = gst_query_new_duration (GST_FORMAT_TIME);

  if (gst_element_query (priv->playbin, position_q) &&
      gst_element_query (priv->playbin, duration_q))
    {
      gint64 position, duration;

      position = duration = 0;

      gst_query_parse_position (position_q, NULL, &position);
      gst_query_parse_duration (duration_q, NULL, &duration);

      progress = CLAMP ((gdouble) position / (gdouble) duration, 0.0, 1.0);
    }
  else
    progress = 0.0;
  
  gst_query_unref (position_q);
  gst_query_unref (duration_q);

  return progress;
}

static void
set_audio_volume (ClutterGstVideoTexture *video_texture,
                  gdouble                 volume)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->playbin)
    return;

  /* the :volume property is in the [0, 10] interval */
  g_object_set (G_OBJECT (priv->playbin), "volume", volume * 10.0, NULL);
  g_object_notify (G_OBJECT (video_texture), "audio-volume");
}

static gdouble
get_audio_volume (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  gdouble volume = 0.0;

  if (!priv->playbin)
    return 0.0;

  /* the :volume property is in the [0, 10] interval */
  g_object_get (priv->playbin, "volume", &volume, NULL);

  return CLAMP (volume / 10.0, 0.0, 1.0);
}

static void
clutter_media_init (ClutterMediaIface *iface)
{
}

static void
clutter_gst_video_texture_dispose (GObject *object)
{
  ClutterGstVideoTexture        *self;
  ClutterGstVideoTexturePrivate *priv; 

  self = CLUTTER_GST_VIDEO_TEXTURE(object); 
  priv = self->priv;

  /* FIXME: flush an errors off bus ? */
  /* gst_bus_set_flushing (priv->bus, TRUE); */

  if (priv->playbin) 
    {
      gst_element_set_state (priv->playbin, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (priv->playbin));
      priv->playbin = NULL;
    }

  if (priv->tick_timeout_id > 0) 
    {
      g_source_remove (priv->tick_timeout_id);
      priv->tick_timeout_id = 0;
    }

  G_OBJECT_CLASS (clutter_gst_video_texture_parent_class)->dispose (object);
}

static void
clutter_gst_video_texture_finalize (GObject *object)
{
  ClutterGstVideoTexture        *self;
  ClutterGstVideoTexturePrivate *priv; 

  self = CLUTTER_GST_VIDEO_TEXTURE (object);
  priv = self->priv;

  g_free (priv->uri);

  G_OBJECT_CLASS (clutter_gst_video_texture_parent_class)->finalize (object);
}

static void
clutter_gst_video_texture_set_property (GObject      *object, 
				        guint         property_id,
				        const GValue *value, 
				        GParamSpec   *pspec)
{
  ClutterGstVideoTexture *video_texture;

  video_texture = CLUTTER_GST_VIDEO_TEXTURE (object);

  switch (property_id)
    {
    case PROP_URI:
      set_uri (video_texture, g_value_get_string (value));
      break;

    case PROP_PLAYING:
      set_playing (video_texture, g_value_get_boolean (value));
      break;

    case PROP_PROGRESS:
      set_progress (video_texture, g_value_get_double (value));
      break;

    case PROP_AUDIO_VOLUME:
      set_audio_volume (video_texture, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_gst_video_texture_get_property (GObject    *object, 
				        guint       property_id,
				        GValue     *value, 
				        GParamSpec *pspec)
{
  ClutterGstVideoTexture *video_texture;
  ClutterGstVideoTexturePrivate *priv;

  video_texture = CLUTTER_GST_VIDEO_TEXTURE (object);
  priv = video_texture->priv;

  switch (property_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;

    case PROP_PLAYING:
      g_value_set_boolean (value, get_playing (video_texture));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, get_progress (video_texture));
      break;

    case PROP_AUDIO_VOLUME:
      g_value_set_double (value, get_audio_volume (video_texture));
      break;

    case PROP_CAN_SEEK:
      g_value_set_boolean (value, priv->can_seek);
      break;

    case PROP_BUFFER_FILL:
      g_value_set_double (value, priv->buffer_fill);
      break;

    case PROP_DURATION:
      g_value_set_int (value, priv->duration);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_gst_video_texture_class_init (ClutterGstVideoTextureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterGstVideoTexturePrivate));

  object_class->dispose      = clutter_gst_video_texture_dispose;
  object_class->finalize     = clutter_gst_video_texture_finalize;
  object_class->set_property = clutter_gst_video_texture_set_property;
  object_class->get_property = clutter_gst_video_texture_get_property;

  g_object_class_override_property (object_class,
                                    PROP_URI, "uri");
  g_object_class_override_property (object_class,
                                    PROP_PLAYING, "playing");
  g_object_class_override_property (object_class,
                                    PROP_PROGRESS, "progress");
  g_object_class_override_property (object_class,
                                    PROP_AUDIO_VOLUME, "audio-volume");
  g_object_class_override_property (object_class,
                                    PROP_CAN_SEEK, "can-seek");
  g_object_class_override_property (object_class,
                                    PROP_DURATION, "duration");
  g_object_class_override_property (object_class,
                                    PROP_BUFFER_FILL, "buffer-fill");
}

static void
bus_message_error_cb (GstBus                 *bus,
                      GstMessage             *message,
                      ClutterGstVideoTexture *video_texture)
{
  GError *error = NULL;

  gst_message_parse_error (message, &error, NULL);
        
  g_signal_emit_by_name (video_texture, "error", error);

  g_error_free (error);
}

static void
bus_message_eos_cb (GstBus                 *bus,
                    GstMessage             *message,
                    ClutterGstVideoTexture *video_texture)
{
  g_object_notify (G_OBJECT (video_texture), "progress");

  g_signal_emit_by_name (video_texture, "eos");
}

static void
bus_message_buffering_cb (GstBus                 *bus,
                          GstMessage             *message,
                          ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  const GstStructure *str;
  gint buffer_percent;
  gboolean res;

  str = gst_message_get_structure (message);
  if (!str)
    return;

  res = gst_structure_get_int (str, "buffer-percent", &buffer_percent);
  if (res)
    {
      priv->buffer_fill = CLAMP ((gdouble) buffer_percent / 100.0,
                                 0.0,
                                 1.0);

      g_object_notify (G_OBJECT (video_texture), "buffer-fill");
    }
}

static void
bus_message_duration_cb (GstBus                 *bus,
                         GstMessage             *message,
                         ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstFormat format;
  gint64 duration;

  gst_message_parse_duration (message, &format, &duration);
  if (format != GST_FORMAT_TIME)
    return;
  
  priv->duration = duration / GST_SECOND;

  g_object_notify (G_OBJECT (video_texture), "duration");
}

static void
bus_message_state_change_cb (GstBus                 *bus,
                             GstMessage             *message,
                             ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstState old_state, new_state;
  gpointer src;

  src = GST_MESSAGE_SRC (message);
  if (src != priv->playbin)
    return;

  gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

  if (old_state == GST_STATE_READY && 
      new_state == GST_STATE_PAUSED) 
    {
      GstQuery *query;

      /* Determine whether we can seek */
      query = gst_query_new_seeking (GST_FORMAT_TIME);

      if (gst_element_query (priv->playbin, query))
        {
          gboolean can_seek = FALSE;

          gst_query_parse_seeking (query, NULL, &can_seek,
                                   NULL,
                                   NULL);

          priv->can_seek = (can_seek == TRUE) ? TRUE : FALSE;
        }
      else
        {
	  /* could not query for ability to seek by querying the
           * playbin; let's crudely try by using the URI
	   */
	  if (priv->uri && g_str_has_prefix (priv->uri, "http://"))
            priv->can_seek = FALSE;
          else
            priv->can_seek = TRUE;
	}

      gst_query_unref (query);

      g_object_notify (G_OBJECT (video_texture), "can-seek");
      
      /* Determine the duration */
      query = gst_query_new_duration (GST_FORMAT_TIME);

      if (gst_element_query (priv->playbin, query)) 
	{
	  gint64 duration;

	  gst_query_parse_duration (query, NULL, &duration);
	  priv->duration = duration / GST_SECOND;

	  g_object_notify (G_OBJECT (video_texture), "duration");
	}

      gst_query_unref (query);
    }
}

static gboolean
lay_pipeline (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstElement *audio_sink = NULL;
  GstElement *video_sink = NULL;

  priv->playbin = gst_element_factory_make ("playbin", "playbin");
  if (!priv->playbin) 
    {
      g_critical ("Unable to create playbin element");
      return FALSE;
    }

  /* ugh - let's go through the audio sinks
   *
   * FIXME - there must be a way to ask gstreamer to do this for us
   */
  audio_sink = gst_element_factory_make ("gconfaudiosink", "audio-sink");
  if (!audio_sink) 
    {
      audio_sink = gst_element_factory_make ("autoaudiosink", "audio-sink");
      if (!audio_sink) 
	{
	  audio_sink = gst_element_factory_make ("alsasink", "audio-sink");
	  g_warning ("Could not create a GST audio_sink. "
		     "Audio unavailable.");

          /* do we even need to bother? */
	  if (!audio_sink)
	    audio_sink = gst_element_factory_make ("fakesink", "audio-sink");
	}
    }

  video_sink = clutter_gst_video_sink_new (CLUTTER_TEXTURE (video_texture));
  g_object_set (G_OBJECT (video_sink), "qos", TRUE, "sync", TRUE, NULL);
  g_object_set (G_OBJECT (priv->playbin),
		"video-sink", video_sink,
		"audio-sink", audio_sink,
		NULL);

  return TRUE;
}

static void
clutter_gst_video_texture_init (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv;
  GstBus *bus;

  video_texture->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (video_texture,
                                 CLUTTER_GST_TYPE_VIDEO_TEXTURE,
                                 ClutterGstVideoTexturePrivate);

  if (!lay_pipeline (video_texture))
    {
      g_warning ("Failed to initiate suitable playback pipeline.");
      return;
    }

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->playbin));

  gst_bus_add_signal_watch (bus);

  g_signal_connect_object (bus, "message::error",
			   G_CALLBACK (bus_message_error_cb),
			   video_texture, 0);
  g_signal_connect_object (bus, "message::eos",
			   G_CALLBACK (bus_message_eos_cb),
			   video_texture, 0);
  g_signal_connect_object (bus, "message::buffering",
			   G_CALLBACK (bus_message_buffering_cb),
			   video_texture, 0);
  g_signal_connect_object (bus, "message::duration",
			   G_CALLBACK (bus_message_duration_cb),
			   video_texture, 0);
  g_signal_connect_object (bus, "message::state-changed",
			   G_CALLBACK (bus_message_state_change_cb),
			   video_texture, 0);

  gst_object_unref (GST_OBJECT (bus));
}

/**
 * clutter_gst_video_texture_new:
 *
 * Creates a video texture.
 *
 * Return value: the newly created video texture actor
 */
ClutterActor*
clutter_gst_video_texture_new (void)
{
  return g_object_new (CLUTTER_GST_TYPE_VIDEO_TEXTURE,
                       "disable-slicing", TRUE, 
                       NULL);
}

/**
 * clutter_gst_video_texture_get_playbin:
 * @texture: a #ClutterGstVideoTexture
 *
 * Retrieves the #GstElement used by the @texture, for direct use with
 * GStreamer API.
 *
 * Return value: the playbin element used by the video texture
 */
GstElement *
clutter_gst_video_texture_get_playbin (ClutterGstVideoTexture *texture)
{
  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture), NULL);

  return texture->priv->playbin;
}
