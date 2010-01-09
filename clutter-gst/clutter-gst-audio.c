/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-audio.c - Audio streaming object.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen <jorn@openedhand.com>
 *             Emmanuele Bassi <ebassi@linux.intel.com>
 *             Damien Lespiau <damien.lespiau@intel.com>
 *
 * Copyright (C) 2006 OpenedHand
 * Copyright (C) 2009, 2010 Intel Corporation
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
 * SECTION:clutter-gst-audio
 * @short_description: Object for playback of audio files.
 *
 * #ClutterGstAudio is an object that plays audio files. It is a simple
 * wrapper around GStreamer base audio sink implementing the #ClutterMedia
 * interface and providing a nice API to play a sound file using its URI.
 */

#include "clutter-gst-audio.h"

#include <gst/gst.h>
#include <gst/audio/gstbaseaudiosink.h>

#include <glib.h>

#include "clutter-gst-debug.h"

struct _ClutterGstAudioPrivate
{
  GstElement *pipeline;

  gchar *uri;

  guint can_seek : 1;

  gdouble buffer_fill;
  gdouble duration;

  guint tick_timeout_id;
};

enum
{
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

G_DEFINE_TYPE_WITH_CODE (ClutterGstAudio,
                         clutter_gst_audio,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

/* Interface implementation */

static gboolean
tick_timeout (gpointer data)
{
  GObject *audio = data;

  g_object_notify (audio, "progress");

  return TRUE;
}

static void
query_duration (ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  gboolean success;
  GstFormat format = GST_FORMAT_TIME;
  gint64 duration;
  gdouble new_duration, difference;

  success = gst_element_query_duration (priv->pipeline, &format, &duration);
  if (G_UNLIKELY (success != TRUE))
    return;

  new_duration = (gdouble) duration / GST_SECOND;

  /* while we store the new duration if it sligthly changes, the duration
   * signal is sent only if the new duration is at least one second different
   * from the old one (as the duration signal is mainly used to update the
   * time displayed in a UI */
  difference = ABS (priv->duration - new_duration);
  if (difference > 1e-3)
    {
      CLUTTER_GST_NOTE (MEDIA, "duration: %.02f", new_duration);
      priv->duration = new_duration;

      if (difference > 1.0)
        g_object_notify (G_OBJECT (audio), "duration");
    }
}

static void
set_uri (ClutterGstAudio *audio,
	 const gchar     *uri)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  GObject *self = G_OBJECT (audio);
  GstState state, pending;

  if (!priv->pipeline)
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
            g_timeout_add (TICK_TIMEOUT * 1000, tick_timeout, audio);
        }
    }
  else 
    {
      priv->uri = NULL;
    
      if (priv->tick_timeout_id > 0) 
	{
	  g_source_remove (priv->tick_timeout_id);
	  priv->tick_timeout_id = 0;
	}
    }

  priv->can_seek = FALSE;
  priv->duration = 0.0;

  gst_element_get_state (priv->pipeline, &state, &pending, 0);

  if (pending)
    state = pending;

  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  
  CLUTTER_GST_NOTE (MEDIA, "setting URI: %s", uri);

  g_object_set (priv->pipeline, "uri", uri, NULL);

  /* Restore state */
  if (uri) 
    gst_element_set_state (priv->pipeline, state);

  /* Emit notififications for all these to make sure UI is not showing
   * any properties of the old URI.
   */
  g_object_notify (self, "uri");
  g_object_notify (self, "can-seek");
  g_object_notify (self, "duration");
  g_object_notify (self, "progress");
}

static void
set_playing (ClutterGstAudio *audio,
	     gboolean         playing)
{
  ClutterGstAudioPrivate *priv = audio->priv;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set playing: %d", playing);
        
  if (priv->uri) 
    {
      GstState state = GST_STATE_PAUSED;
    
      if (playing)
	state = GST_STATE_PLAYING;
    
      gst_element_set_state (priv->pipeline, state);
    }
  else 
    {
      if (playing)
	g_warning ("Unable to start playing: no URI is set");
    }
  
  g_object_notify (G_OBJECT (audio), "playing");
  g_object_notify (G_OBJECT (audio), "progress");
}

static gboolean
get_playing (ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  GstState state, pending;
  gboolean playing;

  if (!priv->pipeline)
    return FALSE;
  
  gst_element_get_state (priv->pipeline, &state, &pending, 0);
  
  if (pending)
    playing = (pending == GST_STATE_PLAYING);
  else
    playing = (state == GST_STATE_PLAYING);

  CLUTTER_GST_NOTE (MEDIA, "get playing: %d", playing);

  return playing;
}

static void
set_progress (ClutterGstAudio *audio,
	      gdouble          progress)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  GstState state, pending;
  GstQuery *duration_q;
  gint64 position;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set progress: %.02f", progress);

  gst_element_get_state (priv->pipeline, &state, &pending, 0);

  if (pending)
    state = pending;

  gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);

  duration_q = gst_query_new_duration (GST_FORMAT_TIME);

  if (gst_element_query (priv->pipeline, duration_q))
    {
      gint64 duration = 0;

      gst_query_parse_duration (duration_q, NULL, &duration);

      position = progress * duration;
    }
  else
    position = 0;

  gst_query_unref (duration_q);

  gst_element_seek (priv->pipeline,
		    1.0,
		    GST_FORMAT_TIME,
		    GST_SEEK_FLAG_FLUSH,
		    GST_SEEK_TYPE_SET,
		    position,
		    0, 0);

  gst_element_set_state (priv->pipeline, state);

  g_object_notify (G_OBJECT (audio), "progress");
}

static gdouble
get_progress (ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  GstQuery *position_q, *duration_q;
  gdouble progress;

  if (!priv->pipeline)
    return 0.0;

  position_q = gst_query_new_position (GST_FORMAT_TIME);
  duration_q = gst_query_new_duration (GST_FORMAT_TIME);

  if (gst_element_query (priv->pipeline, position_q) &&
      gst_element_query (priv->pipeline, duration_q))
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

  CLUTTER_GST_NOTE (MEDIA, "get progress: %.02f", progress);

  return progress;
}

static void
set_audio_volume (ClutterGstAudio *audio,
                  gdouble          volume)
{
  ClutterGstAudioPrivate *priv = audio->priv;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set volume: %.02f", volume);

  /* the :volume property is in the [0, 10] interval */
  g_object_set (G_OBJECT (priv->pipeline), "volume", volume * 10.0, NULL);

  g_object_notify (G_OBJECT (audio), "audio-volume");
}

static gdouble
get_audio_volume (ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  gdouble volume = 0.0;

  if (!priv->pipeline)
    return 0.0;

  /* the :volume property is in the [0, 10] interval */
  g_object_get (priv->pipeline, "volume", &volume, NULL);

  CLUTTER_GST_NOTE (MEDIA, "get volume: %.02f", volume);

  return CLAMP (volume / 10.0, 0.0, 1.0);
}

static void
clutter_media_init (ClutterMediaIface *iface)
{
}

static void
clutter_gst_audio_dispose (GObject *object)
{
  ClutterGstAudio        *self;
  ClutterGstAudioPrivate *priv; 

  self = CLUTTER_GST_AUDIO(object); 
  priv = self->priv;

  /* FIXME: flush an errors off bus ? */
  /* gst_bus_set_flushing (priv->bus, TRUE); */

  if (priv->pipeline)
    {
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (priv->pipeline));
      priv->pipeline = NULL;
    }

  if (priv->tick_timeout_id > 0) 
    {
      g_source_remove (priv->tick_timeout_id);
      priv->tick_timeout_id = 0;
    }

  G_OBJECT_CLASS (clutter_gst_audio_parent_class)->dispose (object);
}

static void
clutter_gst_audio_finalize (GObject *object)
{
  ClutterGstAudio        *self;
  ClutterGstAudioPrivate *priv; 

  self = CLUTTER_GST_AUDIO(object); 
  priv = self->priv;

  g_free (priv->uri);

  G_OBJECT_CLASS (clutter_gst_audio_parent_class)->finalize (object);
}

static void
clutter_gst_audio_set_property (GObject      *object, 
			        guint         property_id,
				const GValue *value, 
				GParamSpec   *pspec)
{
  ClutterGstAudio *audio;

  audio = CLUTTER_GST_AUDIO(object);

  switch (property_id)
    {
    case PROP_URI:
      set_uri (audio, g_value_get_string (value));
      break;

    case PROP_PLAYING:
      set_playing (audio, g_value_get_boolean (value));
      break;

    case PROP_PROGRESS:
      set_progress (audio, g_value_get_double (value));
      break;

    case PROP_AUDIO_VOLUME:
      set_audio_volume (audio, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_gst_audio_get_property (GObject    *object, 
				guint       property_id,
				GValue     *value, 
				GParamSpec *pspec)
{
  ClutterGstAudio *audio = CLUTTER_GST_AUDIO (object);
  ClutterGstAudioPrivate *priv = audio->priv;

  switch (property_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;

    case PROP_PLAYING:
      g_value_set_boolean (value, get_playing (audio));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, get_progress (audio));
      break;

    case PROP_AUDIO_VOLUME:
      g_value_set_double (value, get_audio_volume (audio));
      break;

    case PROP_CAN_SEEK:
      g_value_set_boolean (value, priv->can_seek);
      break;

    case PROP_BUFFER_FILL:
      g_value_set_double (value, priv->buffer_fill);
      break;

    case PROP_DURATION:
      g_value_set_double (value, priv->duration);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_gst_audio_class_init (ClutterGstAudioClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterGstAudioPrivate));

  object_class->dispose      = clutter_gst_audio_dispose;
  object_class->finalize     = clutter_gst_audio_finalize;
  object_class->set_property = clutter_gst_audio_set_property;
  object_class->get_property = clutter_gst_audio_get_property;

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
bus_message_error_cb (GstBus          *bus,
                      GstMessage      *message,
                      ClutterGstAudio *audio)
{
  GError *error;

  error = NULL;
  gst_message_parse_error (message, &error, NULL);
        
  g_signal_emit_by_name (CLUTTER_MEDIA(audio), "error", error);

  g_error_free (error);
}

static void
bus_message_eos_cb (GstBus          *bus,
                    GstMessage      *message,
                    ClutterGstAudio *audio)
{
  g_object_notify (G_OBJECT (audio), "progress");
  
  CLUTTER_GST_NOTE (MEDIA, "EOS");

  g_signal_emit_by_name (CLUTTER_MEDIA(audio), "eos");

  gst_element_set_state (audio->priv->pipeline, GST_STATE_READY);
}

static void
bus_message_buffering_cb (GstBus          *bus,
                          GstMessage      *message,
                          ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
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

      CLUTTER_GST_NOTE (MEDIA, "buffer-fill: %.02f", priv->buffer_fill);

      g_object_notify (G_OBJECT (audio), "buffer-fill");
    }
}

static void
bus_message_duration_cb (GstBus          *bus,
                         GstMessage      *message,
                         ClutterGstAudio *audio)
{
  gint64 duration;

  /* GstElements send a duration message on the bus with GST_CLOCK_TIME_NONE
   * as duration to signal a new duration */
  gst_message_parse_duration (message, NULL, &duration);
  if (G_UNLIKELY (duration != GST_CLOCK_TIME_NONE))
    return;

  query_duration (audio);
}

static void
bus_message_state_change_cb (GstBus          *bus,
                             GstMessage      *message,
                             ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  GstState old_state, new_state;
  gpointer src;

  src = GST_MESSAGE_SRC (message);
  if (src != audio->priv->pipeline)
    return;

  gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

  if (old_state == GST_STATE_READY && 
      new_state == GST_STATE_PAUSED) 
    {
      GstQuery *query;
      
      /* Determine whether we can seek */
      query = gst_query_new_seeking (GST_FORMAT_TIME);
      
      if (gst_element_query (audio->priv->pipeline, query))
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
           * pipeline; let's crudely try by using the URI
	   */
          if (priv->uri && g_str_has_prefix (priv->uri, "http://"))
            priv->can_seek = FALSE;
          else
            priv->can_seek = TRUE;
	}

      gst_query_unref (query);
      
      CLUTTER_GST_NOTE (MEDIA, "can-seek: %d", priv->can_seek);

      g_object_notify (G_OBJECT (audio), "can-seek");
      
      query_duration (audio);
    }
}

static gboolean
lay_pipeline (ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv = audio->priv;
  GstElement *audio_sink = NULL;

  priv->pipeline = gst_element_factory_make ("playbin2", "pipeline");

  if (!priv->pipeline)
    {
      g_warning ("Unable to create playbin2 element");
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

  g_object_set (G_OBJECT (priv->pipeline), "audio-sink", audio_sink, NULL);

  return TRUE;
}

static void
clutter_gst_audio_init (ClutterGstAudio *audio)
{
  ClutterGstAudioPrivate *priv;
  GstBus *bus;

  audio->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (audio, CLUTTER_GST_TYPE_AUDIO,
                                 ClutterGstAudioPrivate);

  if (!lay_pipeline (audio))
    {
      g_warning("Failed to initiate suitable playback pipeline.");
      return;
    }

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));

  gst_bus_add_signal_watch (bus);

  g_signal_connect_object (bus, "message::error",
			   G_CALLBACK (bus_message_error_cb),
			   audio, 0);
  g_signal_connect_object (bus, "message::eos",
			   G_CALLBACK (bus_message_eos_cb),
			   audio, 0);
  g_signal_connect_object (bus, "message::buffering",
			   G_CALLBACK (bus_message_buffering_cb),
			   audio, 0);
  g_signal_connect_object (bus, "message::duration",
			   G_CALLBACK (bus_message_duration_cb),
			   audio, 0);
  g_signal_connect_object (bus, "message::state-changed",
			   G_CALLBACK (bus_message_state_change_cb),
			   audio, 0);

  gst_object_unref (GST_OBJECT (bus));
}

/**
 * clutter_gst_audio_new:
 *
 * Creates #ClutterGstAudio object.
 *
 * Return value: A newly allocated #ClutterGstAudio object.
 */
ClutterGstAudio *
clutter_gst_audio_new (void)
{
  return g_object_new (CLUTTER_GST_TYPE_AUDIO, NULL);
}

/**
 * clutter_gst_audio_get_pipeline:
 * @audio: a #ClutterGstAudio
 *
 * Retrieves the #GstElement used by the @audio, for direct use with
 * GStreamer API.
 *
 * Return value: the pipeline element used by the audio object
 */
GstElement *
clutter_gst_audio_get_pipeline (ClutterGstAudio *audio)
{
  g_return_val_if_fail (CLUTTER_GST_IS_AUDIO (audio), NULL);

  return audio->priv->pipeline;
}
