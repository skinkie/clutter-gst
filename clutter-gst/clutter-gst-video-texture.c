/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-video-texture.c - ClutterTexture using GStreamer to display a
 *                               video stream.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Damien Lespiau <damien.lespiau@intel.com>
 *
 * Copyright (C) 2006 OpenedHand
 * Copyright (C) 2010 Intel Corporation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <gst/gst.h>

#include "clutter-gst-debug.h"
#include "clutter-gst-private.h"
#include "clutter-gst-video-sink.h"
#include "clutter-gst-video-texture.h"

struct _ClutterGstVideoTexturePrivate
{
  GstElement *pipeline;

  gchar *uri;

  guint can_seek : 1;
  guint in_seek : 1;
  guint is_idle : 1;
  gdouble stacked_progress;
  GstState stacked_state;

  guint tick_timeout_id;

  gdouble buffer_fill;
  gdouble duration;
  gchar *font_name;
  gchar *user_agent;

  CoglHandle idle_material;
  CoglColor idle_color_unpre;
};

enum {
  PROP_0,

  /* ClutterMedia properties */
  PROP_URI,
  PROP_PLAYING,
  PROP_PROGRESS,
  PROP_SUBTITLE_URI,
  PROP_SUBTITLE_FONT_NAME,
  PROP_AUDIO_VOLUME,
  PROP_CAN_SEEK,
  PROP_BUFFER_FILL,
  PROP_DURATION,

  PROP_IDLE_MATERIAL,
  PROP_USER_AGENT
};


#define TICK_TIMEOUT 0.5

static void clutter_media_init (ClutterMediaIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterGstVideoTexture,
                         clutter_gst_video_texture,
                         CLUTTER_TYPE_TEXTURE,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

/* Clutter 1.4 has this symbol, we don't want to depend on 1.4 just for that
 * just yet */
static void
_cogl_color_unpremultiply (CoglColor *color)
{
  gfloat alpha;

  alpha = cogl_color_get_alpha (color);

  if (alpha != 0)
    {
      gfloat red, green, blue;

      red = cogl_color_get_red (color);
      green = cogl_color_get_green (color);
      blue = cogl_color_get_blue (color);

      red = red / alpha;
      green = green / alpha;
      blue = blue / alpha;

      cogl_color_set_from_4f (color, red, green, blue, alpha);
    }
}

/* Clutter 1.4 has this symbol, we don't want to depend on 1.4 just for that
 * just yet */
static void
_cogl_color_set_alpha_byte (CoglColor     *color,
                            unsigned char  alpha)
{
  unsigned char red, green, blue;

  red = cogl_color_get_red_byte (color);
  green = cogl_color_get_green_byte (color);
  blue = cogl_color_get_blue_byte (color);

  cogl_color_set_from_4ub (color, red, green, blue, alpha);
}

static void
gen_texcoords_and_draw_cogl_rectangle (ClutterActor *self)
{
  ClutterActorBox box;

  clutter_actor_get_allocation_box (self, &box);

  cogl_rectangle_with_texture_coords (0, 0,
                                      box.x2 - box.x1,
                                      box.y2 - box.y1,
                                      0, 0, 1.0, 1.0);
}

static void
create_black_idle_material (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  priv->idle_material = cogl_material_new ();
  cogl_color_set_from_4ub (&priv->idle_color_unpre, 0, 0, 0, 0xff);
  cogl_material_set_color (priv->idle_material, &priv->idle_color_unpre);
}

static void
set_user_agent (ClutterGstVideoTexture *video_texture,
                const gchar            *user_agent)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstElement *source;
  GParamSpec *pspec;

  if (user_agent == NULL)
    return;

  g_object_get (priv->pipeline, "source", &source, NULL);
  if (source == NULL)
    return;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (source),
                                        "user-agent");
  if (pspec == NULL)
    return;

  CLUTTER_GST_NOTE (MEDIA, "setting user agent: %s", user_agent);

  g_object_set (source, "user-agent", user_agent, NULL);
}

/*
 * ClutterMedia implementation
 */

static gboolean
tick_timeout (gpointer data)
{
  GObject *video_texture = data;

  g_object_notify (video_texture, "progress");

  return TRUE;
}

static void
autoload_subtitle (ClutterGstVideoTexture *video_texture,
                   const gchar            *uri)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  gchar *path, *dot, *subtitle_path;
  GFile *video;
  guint i;

  static const char subtitles_extensions[][4] =
    {
      "sub", "SUB",
      "srt", "SRT",
      "smi", "SMI",
      "ssa", "SSA",
      "ass", "ASS",
      "asc", "ASC"
    };

  /* do not try to look for subtitle files if the video file is not mounted
   * locally */
  if (!g_str_has_prefix (uri, "file://"))
    return;

  /* Retrieve the absolute path of the video file */
  video = g_file_new_for_uri (uri);
  path = g_file_get_path (video);
  g_object_unref (video);
  if (path == NULL)
    return;

  /* Put a '\0' after the dot of the extension */
  dot = strrchr (path, '.');
  if (dot == NULL) {
    g_free (path);
    return;
  }
  *++dot = '\0';

  /* we can't use path as the temporary buffer for the paths of the potential
   * subtitle files as we may not have enough room there */
  subtitle_path = g_malloc (strlen (path) + 1 + 4);
  strcpy (subtitle_path, path);

  /* reuse dot to point to the first byte of the extension of subtitle_path */
  dot = subtitle_path + (dot - path);

  for (i = 0; i < G_N_ELEMENTS (subtitles_extensions); i++)
    {
      GFile *candidate;

      memcpy (dot, subtitles_extensions[i], 4);
      candidate = g_file_new_for_path (subtitle_path);
      if (g_file_query_exists (candidate, NULL))
        {
          gchar *suburi;

          suburi = g_file_get_uri (candidate);

          CLUTTER_GST_NOTE (MEDIA, "found subtitle: %s", suburi);

          g_object_set (priv->pipeline, "suburi", suburi, NULL);
          g_free (suburi);

          g_object_unref (candidate);
          break;
        }

      g_object_unref (candidate);
    }

  g_free (path);
  g_free (subtitle_path);
}

static void
query_duration (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
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
        g_object_notify (G_OBJECT (video_texture), "duration");
    }
}

static void
set_uri (ClutterGstVideoTexture *video_texture,
         const gchar            *uri)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GObject *self = G_OBJECT (video_texture);
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
            g_timeout_add (TICK_TIMEOUT * 1000, tick_timeout, self);
        }

      /* try to load subtitles based on the uri of the file */
      autoload_subtitle (video_texture, uri);
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
  priv->duration = 0.0;

  gst_element_get_state (priv->pipeline, &state, &pending, 0);

  if (pending)
    state = pending;

  gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  CLUTTER_GST_NOTE (MEDIA, "setting URI: %s", uri);

  g_object_set (priv->pipeline, "uri", uri, NULL);

  /*
   * Restore state.
   */
  if (uri)
    gst_element_set_state (priv->pipeline, state);

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

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set playing: %d", playing);

  if (priv->uri) 
    {
      GstState state = GST_STATE_PAUSED;

      if (playing)
	state = GST_STATE_PLAYING;

      priv->in_seek = FALSE;

      gst_element_set_state (priv->pipeline, state);
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
set_progress (ClutterGstVideoTexture *video_texture,
              gdouble                 progress)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstState pending;
  GstQuery *duration_q;
  gint64 position;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set progress: %.02f", progress);

  if (priv->in_seek)
    {
      CLUTTER_GST_NOTE (MEDIA, "already seeking. stacking progress point.");
      priv->stacked_progress = progress;
      return;
    }

  gst_element_get_state (priv->pipeline, &priv->stacked_state, &pending, 0);

  if (pending)
    priv->stacked_state = pending;

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
		    GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
		    GST_SEEK_TYPE_SET,
		    position,
		    GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  priv->in_seek = TRUE;
  priv->stacked_progress = 0.0;
}

static gdouble
get_progress (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
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
set_subtitle_uri (ClutterGstVideoTexture *video_texture,
                  const gchar            *uri)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "setting subtitle URI: %s", uri);

  g_object_set (priv->pipeline, "suburi", uri, NULL);
}

static void
set_subtitle_font_name (ClutterGstVideoTexture *video_texture,
                        const gchar            *font_name)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "setting subtitle font to %s", font_name);

  g_free (priv->font_name);
  priv->font_name = g_strdup (font_name);
  g_object_set (priv->pipeline, "subtitle-font-desc", font_name, NULL);
}

static void
set_audio_volume (ClutterGstVideoTexture *video_texture,
                  gdouble                 volume)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set volume: %.02f", volume);

  /* the :volume property is in the [0, 10] interval */
  g_object_set (G_OBJECT (priv->pipeline), "volume", volume * 10.0, NULL);
  g_object_notify (G_OBJECT (video_texture), "audio-volume");
}

static gdouble
get_audio_volume (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  gdouble volume = 0.0;

  if (!priv->pipeline)
    return 0.0;

  /* the :volume property is in the [0, 10] interval */
  g_object_get (priv->pipeline, "volume", &volume, NULL);

  volume = CLAMP (volume / 10.0, 0.0, 1.0);

  CLUTTER_GST_NOTE (MEDIA, "get volume: %.02f", volume);

  return volume;
}

static void
clutter_media_init (ClutterMediaIface *iface)
{
}

/*
 * Clutter actor implementation
 */

/*
 * ClutterTexture unconditionnaly sets the material color to:
 *    (opacity,opacity,opacity,opacity)
 * so we can't set a black material to the texture. Let's override paint()
 * for now.
 */
static void
clutter_gst_video_texture_paint (ClutterActor *actor)
{
  ClutterGstVideoTexture *video_texture = (ClutterGstVideoTexture *) actor;
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  ClutterActorClass *actor_class;

  if (G_UNLIKELY (priv->is_idle))
    {
      CoglColor *color;
      gfloat alpha;

      /* blend the alpha of the idle material with the actor's opacity */
      color = cogl_color_copy (&priv->idle_color_unpre);
      alpha = clutter_actor_get_paint_opacity (actor) *
              cogl_color_get_alpha_byte (color) / 0xff;
      _cogl_color_set_alpha_byte (color, alpha);
      cogl_color_premultiply (color);
      cogl_material_set_color (priv->idle_material, color);

      cogl_set_source (priv->idle_material);

      /* draw */
      gen_texcoords_and_draw_cogl_rectangle (actor);
    }
  else
    {
      /* when not idle, just chain up to ClutterTexture::paint() */
      actor_class =
        CLUTTER_ACTOR_CLASS (clutter_gst_video_texture_parent_class);
      actor_class->paint (actor);
    }

}

/*
 * GObject implementation
 */

static void
clutter_gst_video_texture_dispose (GObject *object)
{
  ClutterGstVideoTexture        *self;
  ClutterGstVideoTexturePrivate *priv; 

  self = CLUTTER_GST_VIDEO_TEXTURE(object); 
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
  g_free (priv->font_name);
  if (priv->idle_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (priv->idle_material);

  G_OBJECT_CLASS (clutter_gst_video_texture_parent_class)->finalize (object);
}

static void
clutter_gst_video_texture_set_property (GObject      *object, 
				        guint         property_id,
				        const GValue *value, 
				        GParamSpec   *pspec)
{
  ClutterGstVideoTexture *video_texture = CLUTTER_GST_VIDEO_TEXTURE (object);

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

    case PROP_SUBTITLE_URI:
      set_subtitle_uri (video_texture, g_value_get_string (value));
      break;

    case PROP_SUBTITLE_FONT_NAME:
      set_subtitle_font_name (video_texture, g_value_get_string (value));
      break;

    case PROP_AUDIO_VOLUME:
      set_audio_volume (video_texture, g_value_get_double (value));
      break;

    case PROP_IDLE_MATERIAL:
      clutter_gst_video_texture_set_idle_material (video_texture,
                                                   g_value_get_boxed (value));
      break;

    case PROP_USER_AGENT:
      clutter_gst_video_texture_set_user_agent (video_texture,
                                                g_value_get_string (value));
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
  char *str;

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

    case PROP_SUBTITLE_URI:
      g_object_get (priv->pipeline, "suburi", &str, NULL);
      g_value_take_string (value, str);
      break;

    case PROP_SUBTITLE_FONT_NAME:
      g_value_set_string (value, priv->font_name);
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
      g_value_set_double (value, priv->duration);
      break;

    case PROP_IDLE_MATERIAL:
      g_value_set_boxed (value, priv->idle_material);
      break;

    case PROP_USER_AGENT:
      {
        gchar *user_agent;

        user_agent = clutter_gst_video_texture_get_user_agent (video_texture);
        g_value_take_string (value, user_agent);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
clutter_gst_video_texture_class_init (ClutterGstVideoTextureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterGstVideoTexturePrivate));

  object_class->dispose      = clutter_gst_video_texture_dispose;
  object_class->finalize     = clutter_gst_video_texture_finalize;
  object_class->set_property = clutter_gst_video_texture_set_property;
  object_class->get_property = clutter_gst_video_texture_get_property;

  actor_class->paint = clutter_gst_video_texture_paint;

  g_object_class_override_property (object_class,
                                    PROP_URI, "uri");
  g_object_class_override_property (object_class,
                                    PROP_PLAYING, "playing");
  g_object_class_override_property (object_class,
                                    PROP_PROGRESS, "progress");
  g_object_class_override_property (object_class,
                                    PROP_SUBTITLE_URI, "subtitle-uri");
  g_object_class_override_property (object_class,
                                    PROP_SUBTITLE_FONT_NAME,
                                    "subtitle-font-name");
  g_object_class_override_property (object_class,
                                    PROP_AUDIO_VOLUME, "audio-volume");
  g_object_class_override_property (object_class,
                                    PROP_CAN_SEEK, "can-seek");
  g_object_class_override_property (object_class,
                                    PROP_DURATION, "duration");
  g_object_class_override_property (object_class,
                                    PROP_BUFFER_FILL, "buffer-fill");

  pspec = g_param_spec_boxed ("idle-material",
                              "Idle material",
                              "Material to use for drawing when not playing",
                              COGL_TYPE_HANDLE,
                              CLUTTER_GST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_IDLE_MATERIAL, pspec);

  pspec = g_param_spec_string ("user-agent",
                               "User Agent",
                               "User Agent used with network protocols",
                               NULL,
                               CLUTTER_GST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_USER_AGENT, pspec);
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

  CLUTTER_GST_NOTE (MEDIA, "EOS");

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

      CLUTTER_GST_NOTE (MEDIA, "buffer-fill: %.02f", priv->buffer_fill);

      g_object_notify (G_OBJECT (video_texture), "buffer-fill");
    }
}

static void
bus_message_duration_cb (GstBus                 *bus,
                         GstMessage             *message,
                         ClutterGstVideoTexture *video_texture)
{
  gint64 duration;

  /* GstElements send a duration message on the bus with GST_CLOCK_TIME_NONE
   * as duration to signal a new duration */
  gst_message_parse_duration (message, NULL, &duration);
  if (G_UNLIKELY (duration != GST_CLOCK_TIME_NONE))
    return;

  query_duration (video_texture);
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
  if (src != priv->pipeline)
    return;

  gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

  if (old_state == GST_STATE_READY && 
      new_state == GST_STATE_PAUSED) 
    {
      GstQuery *query;

      /* Determine whether we can seek */
      query = gst_query_new_seeking (GST_FORMAT_TIME);

      if (gst_element_query (priv->pipeline, query))
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

      g_object_notify (G_OBJECT (video_texture), "can-seek");

      query_duration (video_texture);
    }

  /* is_idle controls the drawing with the idle material */
  if (new_state == GST_STATE_NULL)
    priv->is_idle = TRUE;
  else if (new_state == GST_STATE_PLAYING)
    priv->is_idle = FALSE;
}

static void
bus_message_async_done_cb (GstBus                 *bus,
                           GstMessage             *message,
                           ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (priv->in_seek)
    {
      g_object_notify (G_OBJECT (video_texture), "progress");

      priv->in_seek = FALSE;
      gst_element_set_state (priv->pipeline, priv->stacked_state);

      if (priv->stacked_progress)
        {
          set_progress (video_texture, priv->stacked_progress);
        }
    }
}

static void
on_source_changed (GstElement             *pipeline,
                   GParamSpec             *pspec,
                   ClutterGstVideoTexture *video_texture)
{
  set_user_agent (video_texture, video_texture->priv->user_agent);
}

static gboolean
lay_pipeline (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstElement *audio_sink = NULL;
  GstElement *video_sink = NULL;

  priv->pipeline = gst_element_factory_make ("playbin2", "pipeline");
  if (!priv->pipeline) 
    {
      g_critical ("Unable to create playbin2 element");
      return FALSE;
    }

  g_signal_connect (priv->pipeline, "notify::source",
                    G_CALLBACK (on_source_changed), video_texture);

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
  g_object_set (G_OBJECT (priv->pipeline),
                "video-sink", video_sink,
                "audio-sink", audio_sink,
                "subtitle-font-desc", "Sans 16",
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

  create_black_idle_material (video_texture);

  priv->is_idle = TRUE;

  priv->in_seek = FALSE;

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));

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
  g_signal_connect_object (bus, "message::async-done",
                           G_CALLBACK (bus_message_async_done_cb),
                           video_texture, 0);

  gst_object_unref (GST_OBJECT (bus));
}

/**
 * clutter_gst_video_texture_new:
 *
 * Creates a video texture.
 *
 * <note>This function has to be called from Clutter's main thread. While
 * GStreamer will spawn threads to do its work, we want all the GL calls to
 * happen in the same thread. Clutter-gst knows which thread it is by
 * assuming this constructor is called from the Clutter thread.</note>
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
 * clutter_gst_video_texture_get_pipeline:
 * @texture: a #ClutterGstVideoTexture
 *
 * Retrieves the #GstPipeline used by the @texture, for direct use with
 * GStreamer API.
 *
 * Return value: the pipeline element used by the video texture
 */
GstElement *
clutter_gst_video_texture_get_pipeline (ClutterGstVideoTexture *texture)
{
  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture), NULL);

  return texture->priv->pipeline;
}

/**
 * clutter_gst_video_texture_get_idle_material:
 * @texture: a #ClutterGstVideoTexture
 *
 * Retrieves the material used to draw when no media is being played.
 *
 * Return value: the #CoglHandle of the idle material
 *
 * Since: 1.2
 */
CoglHandle
clutter_gst_video_texture_get_idle_material (ClutterGstVideoTexture *texture)
{
  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture),
                        COGL_INVALID_HANDLE);

  return texture->priv->idle_material;
}
/**

 * clutter_gst_video_texture_set_idle_material:
 * @texture: a #ClutterGstVideoTexture
 * @material: the handle of a Cogl material
 *
 * Sets a material to use to draw when no media is being played. The
 * #ClutterGstVideoTexture holds a reference of the @material.
 *
 * The default idle material will paint the #ClutterGstVideoTexture in black.
 * If %COGL_INVALID_HANDLE is given as @material to this function, this
 * default idle material will be used.
 *
 * Since: 1.2
 */
void
clutter_gst_video_texture_set_idle_material (ClutterGstVideoTexture *texture,
                                             CoglHandle              material)
{
  ClutterGstVideoTexturePrivate *priv;

  g_return_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture));

  priv = texture->priv;
  /* priv->idle_material always has a valid material */
  cogl_handle_unref (priv->idle_material);

  if (material != COGL_INVALID_HANDLE)
    {
      priv->idle_material = cogl_handle_ref (material);
      cogl_material_get_color (material, &priv->idle_color_unpre);
      _cogl_color_unpremultiply (&priv->idle_color_unpre);
    }
  else
    {
      create_black_idle_material (texture);
    }

  g_object_notify (G_OBJECT (texture), "idle-material");
}

/**
 * clutter_gst_video_texture_get_user_agent:
 * @texture: a #ClutterGstVideoTexture
 *
 * Retrieves the user agent used when streaming.
 *
 * Return value: the user agent used. The returned string has to be freed with
 * g_free()
 *
 * Since: 1.2
 */
gchar *
clutter_gst_video_texture_get_user_agent (ClutterGstVideoTexture *texture)
{
  ClutterGstVideoTexturePrivate *priv;
  GstElement *source;
  GParamSpec *pspec;
  gchar *user_agent;

  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture), NULL);

  priv = texture->priv;

  /* If the user has set a custom user agent, we just return it even if it is
   * not used by the current source element of the pipeline */
  if (priv->user_agent)
    return g_strdup (priv->user_agent);

  /* If not, we try to retrieve the user agent used by the current source */
  g_object_get (priv->pipeline, "source", &source, NULL);
  if (source == NULL)
    return NULL;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (source),
                                        "user-agent");
  if (pspec == NULL)
    return NULL;

  g_object_get (source, "user-agent", &user_agent, NULL);

  return user_agent;
}

/**
 * clutter_gst_video_texture_set_user_agent:
 * @texture: a #ClutterGstVideoTexture
 * @user_agent: the user agent
 *
 * Sets the user agent to use when streaming.
 *
 * When streaming content, you might want to set a custom user agent, eg. to
 * promote your software, make it appear in statistics or because the server
 * requires a special user agent you want to impersonate.
 *
 * Since: 1.2
 */
void
clutter_gst_video_texture_set_user_agent (ClutterGstVideoTexture *texture,
                                          const gchar *           user_agent)
{
  ClutterGstVideoTexturePrivate *priv;

  g_return_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture));

  priv = texture->priv;
  g_free (priv->user_agent);
  if (user_agent)
    priv->user_agent = g_strdup (user_agent);
  else
    priv->user_agent = NULL;

  set_user_agent (texture, user_agent);
}
