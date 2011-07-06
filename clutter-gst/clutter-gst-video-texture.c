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
 * Copyright (C) 2010, 2011 Intel Corporation
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
#include <gst/video/video.h>
#include <gst/tag/tag.h>
#include <gst/interfaces/streamvolume.h>

#include "clutter-gst-debug.h"
#include "clutter-gst-enum-types.h"
#include "clutter-gst-marshal.h"
#include "clutter-gst-private.h"
#include "clutter-gst-video-sink.h"
#include "clutter-gst-video-texture.h"

/* Elements don't expose header files */
typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2),
  GST_PLAY_FLAG_VIS           = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
  GST_PLAY_FLAG_BUFFERING     = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE   = (1 << 9)
} GstPlayFlags;

enum
{
  DOWNLOAD_BUFFERING,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

struct _ClutterGstVideoTexturePrivate
{
  GstElement *pipeline;

  gchar *uri;

  guint can_seek : 1;
  guint in_seek : 1;
  guint is_idle : 1;
  guint is_changing_uri : 1;
  guint in_error : 1;
  guint in_eos : 1;
  guint in_download_buffering : 1;

  /* when in progressive download, we use the buffer-fill property to signal
   * that we have enough data to play the stream. This flag allows to send
   * the notify that buffer-fill is 1.0 only once */
  guint virtual_stream_buffer_signalled : 1;

  gdouble stacked_progress;

  gdouble target_progress;
  GstState target_state;

  guint tick_timeout_id;
  guint buffering_timeout_id;

  /* width / height (in pixels) of the frame data before applying the pixel
   * aspect ratio */
  gint buffer_width;
  gint buffer_height;

  /* Pixel aspect ration is par_n / par_d. this is set by the sink */
  guint par_n, par_d;

  /* natural width / height (in pixels) of the texture (after par applied) */
  guint texture_width;
  guint texture_height;

  /* This is a cubic volume, suitable for use in a UI cf. StreamVolume doc */
  gdouble volume;

  gdouble buffer_fill;
  gdouble duration;
  gchar *font_name;
  gchar *user_agent;

  CoglHandle idle_material;
  CoglColor idle_color_unpre;

  GstSeekFlags seek_flags;    /* flags for the seek in set_progress(); */

  GstElement *download_buffering_element;

  GList *audio_streams;
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
  PROP_USER_AGENT,
  PROP_SEEK_FLAGS,
  PROP_AUDIO_STREAMS,
  PROP_AUDIO_STREAM
};

/* idle timeouts (in ms) */
#define TICK_TIMEOUT        500
#define BUFFERING_TIMEOUT   250

static void clutter_media_init (ClutterMediaIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterGstVideoTexture,
                         clutter_gst_video_texture,
                         CLUTTER_TYPE_TEXTURE,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_MEDIA,
                                                clutter_media_init));

static const gchar *
gst_state_to_string (GstState state)
{
  switch (state)
    {
    case GST_STATE_VOID_PENDING:
      return "pending";
    case GST_STATE_NULL:
      return "null";
    case GST_STATE_READY:
      return "ready";
    case GST_STATE_PAUSED:
      return "paused";
    case GST_STATE_PLAYING:
      return "playing";
    }

  return "Unknown state";
}

static void set_subtitle_uri (ClutterGstVideoTexture *video_texture,
                              const gchar            *uri);
static void configure_buffering_timeout (ClutterGstVideoTexture *video_texture,
                                         guint                   ms);

static void
free_string_list (GList **listp)
{
  GList *l;

  l = *listp;
  while (l)
    {
      g_free (l->data);
      l = g_list_delete_link (l, l);
    }

  *listp = NULL;
}

static void
clear_download_buffering (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (priv->download_buffering_element)
    {
      g_object_unref (priv->download_buffering_element);
      priv->download_buffering_element = NULL;
    }
  configure_buffering_timeout (video_texture, 0);
  priv->in_download_buffering = FALSE;
  priv->virtual_stream_buffer_signalled = 0;
}

static gboolean
buffering_timeout (gpointer data)
{
  ClutterGstVideoTexture *video_texture = CLUTTER_GST_VIDEO_TEXTURE (data);
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  gdouble start_d, stop_d, seconds_buffered;
  gint64 start, stop, left;
  GstState current_state;
  GstElement *element;
  GstQuery *query;
  gboolean res;

  element = priv->download_buffering_element;
  if (element == NULL)
    element = priv->pipeline;

  /* queue2 only knows about _PERCENT and _BYTES */
  query = gst_query_new_buffering (GST_FORMAT_PERCENT);
  res = gst_element_query (element, query);

  if (res == FALSE)
    {
      priv->buffering_timeout_id = 0;
      clear_download_buffering (video_texture);
      return FALSE;
    }

  /* signal the current range */
  gst_query_parse_buffering_stats (query, NULL, NULL, NULL, &left);
  gst_query_parse_buffering_range (query, NULL, &start, &stop, NULL);

  CLUTTER_GST_NOTE (BUFFERING,
                    "start %" G_GINT64_FORMAT ", stop %" G_GINT64_FORMAT
                    ", buffering left %" G_GINT64_FORMAT, start, stop, left);

  start_d = (gdouble) start / GST_FORMAT_PERCENT_MAX;
  stop_d = (gdouble) stop / GST_FORMAT_PERCENT_MAX;

  g_signal_emit (video_texture,
                 signals[DOWNLOAD_BUFFERING], 0, start_d, stop_d);

  /* handle the "virtual stream buffer" and the associated pipeline state.
   * We pause the pipeline until 2s of content is buffered. With the current
   * implementation of queue2, start is always 0, so even when we seek in
   * the stream the start position of the download-buffering signal is
   * always 0.0. FIXME: look at gst_query_parse_nth_buffering_range () */
  seconds_buffered = priv->duration * (stop_d - start_d);
  priv->buffer_fill = seconds_buffered / 2.0;
  priv->buffer_fill = CLAMP (priv->buffer_fill, 0.0, 1.0);

  if (priv->buffer_fill != 1.0 || !priv->virtual_stream_buffer_signalled)
    {
      CLUTTER_GST_NOTE (BUFFERING, "buffer holds %0.2fs of data, buffer-fill "
                        "is %.02f", seconds_buffered, priv->buffer_fill);

      g_object_notify (G_OBJECT (video_texture), "buffer-fill");

      if (priv->buffer_fill == 1.0)
        priv->virtual_stream_buffer_signalled = 1;
    }

  gst_element_get_state (priv->pipeline, &current_state, NULL, 0);
  if (priv->buffer_fill < 1.0)
    {
      if (current_state != GST_STATE_PAUSED)
        {
          CLUTTER_GST_NOTE (BUFFERING, "pausing the pipeline");
          gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
        }
    }
  else
    {
      if (current_state != priv->target_state)
        {
          CLUTTER_GST_NOTE (BUFFERING, "restoring the pipeline");
          gst_element_set_state (priv->pipeline, priv->target_state);
        }
    }

  /* the file has finished downloading */
  if (left == G_GINT64_CONSTANT (0))
    {
      priv->buffering_timeout_id = 0;

      clear_download_buffering (video_texture);
      gst_query_unref (query);
      return FALSE;
    }

  gst_query_unref (query);
  return TRUE;
}

static void
configure_buffering_timeout (ClutterGstVideoTexture *video_texture,
                             guint                   ms)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (priv->buffering_timeout_id)
    {
      g_source_remove (priv->buffering_timeout_id);
      priv->buffering_timeout_id = 0;
    }

  if (ms)
    {
      priv->buffering_timeout_id =
        g_timeout_add (ms, buffering_timeout, video_texture);
    }
}

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

  CLUTTER_GST_NOTE (MEDIA, "setting uri %s", uri);

  if (!priv->pipeline)
    return;

  g_free (priv->uri);

  priv->in_eos = FALSE;
  priv->in_error = FALSE;

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
            g_timeout_add (TICK_TIMEOUT, tick_timeout, self);
        }

      /* try to load subtitles based on the uri of the file */
      set_subtitle_uri (video_texture, NULL);
      autoload_subtitle (video_texture, uri);

      /* reset the states of download buffering */
      clear_download_buffering (video_texture);
    }
  else
    {
      priv->uri = NULL;

      set_subtitle_uri (video_texture, NULL);

      if (priv->tick_timeout_id)
	{
	  g_source_remove (priv->tick_timeout_id);
	  priv->tick_timeout_id = 0;
	}

      if (priv->buffering_timeout_id)
        {
          g_source_remove (priv->buffering_timeout_id);
          priv->buffering_timeout_id = 0;
        }

      if (priv->download_buffering_element)
        {
          g_object_unref (priv->download_buffering_element);
          priv->download_buffering_element = NULL;
        }

    }

  priv->can_seek = FALSE;
  priv->duration = 0.0;
  priv->stacked_progress = 0.0;
  priv->target_progress = 0.0;

  CLUTTER_GST_NOTE (MEDIA, "setting URI: %s", uri);

  if (uri)
    {
      gst_element_get_state (priv->pipeline, &state, &pending, 0);
      if (pending)
        state = pending;

      gst_element_set_state (priv->pipeline, GST_STATE_NULL);

      g_object_set (priv->pipeline, "uri", uri, NULL);

      gst_element_set_state (priv->pipeline, state);

      priv->is_changing_uri = TRUE;
    }
  else
    {
      priv->is_idle = TRUE;
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      clutter_actor_queue_redraw (CLUTTER_ACTOR (video_texture));
    }

  /*
   * Emit notifications for all these to make sure UI is not showing
   * any properties of the old URI.
   */
  g_object_notify (self, "uri");
  g_object_notify (self, "can-seek");
  g_object_notify (self, "duration");
  g_object_notify (self, "progress");

  free_string_list (&priv->audio_streams);
  CLUTTER_GST_NOTE (AUDIO_STREAM, "audio-streams changed");
  g_object_notify (self, "audio-streams");
}

static void
set_playing (ClutterGstVideoTexture *video_texture,
             gboolean                playing)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set playing: %d", playing);

  priv->in_error = FALSE;
  priv->in_eos = FALSE;

  priv->target_state = playing ? GST_STATE_PLAYING : GST_STATE_PAUSED;

  if (priv->uri)
    {
      priv->in_seek = FALSE;

      gst_element_set_state (priv->pipeline, priv->target_state);
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
  GstQuery *duration_q;
  gint64 position;

  if (!priv->pipeline)
    return;

  CLUTTER_GST_NOTE (MEDIA, "set progress: %.02f", progress);

  priv->in_eos = FALSE;
  priv->target_progress = progress;

  if (priv->in_download_buffering)
    {
      /* we clear the virtual_stream_buffer_signalled flag as it's likely we
       * need to buffer again */
      priv->virtual_stream_buffer_signalled = 0;
    }

  if (priv->in_seek || priv->is_idle || priv->is_changing_uri)
    {
      /* We can't seek right now, let's save the position where we
         want to seek and do that later. */
      CLUTTER_GST_NOTE (MEDIA,
                        "already seeking/idleing. stacking progress point.");
      priv->stacked_progress = progress;
      return;
    }

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
		    GST_SEEK_FLAG_FLUSH | priv->seek_flags,
		    GST_SEEK_TYPE_SET,
		    position,
		    GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  priv->in_seek = TRUE;
  priv->stacked_progress = 0.0;

  CLUTTER_GST_NOTE (MEDIA, "set progress (seeked): %.02f", progress);
}

static gdouble
get_progress (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstQuery *position_q, *duration_q;
  gdouble progress;

  if (!priv->pipeline)
    return 0.0;

  /* when hitting an error or after an EOS, playbin2 has some weird values when
   * querying the duration and progress. We default to 0.0 on error and 1.0 on
   * EOS */
  if (priv->in_error)
    {
      CLUTTER_GST_NOTE (MEDIA, "get progress (error): 0.0");
      return 0.0;
    }

  if (priv->in_eos)
    {
      CLUTTER_GST_NOTE (MEDIA, "get progress (eos): 1.0");
      return 1.0;
    }

  /* When seeking, the progress returned by playbin2 is 0.0. We want that to be
   * the last known position instead as returning 0.0 will have some ugly
   * effects, say on a progress bar getting updated from the progress tick. */
  if (priv->in_seek || priv->is_changing_uri)
    {
      CLUTTER_GST_NOTE (MEDIA, "get progress (target): %.02f",
                        priv->target_progress);
      return priv->target_progress;
    }

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

  CLUTTER_GST_NOTE (MEDIA, "get progress (pipeline): %.02f", progress);

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

  volume = CLAMP (volume, 0.0, 1.0);
  gst_stream_volume_set_volume (GST_STREAM_VOLUME (priv->pipeline),
				GST_STREAM_VOLUME_FORMAT_CUBIC,
				volume);
  g_object_notify (G_OBJECT (video_texture), "audio-volume");
}

static gdouble
get_audio_volume (ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  if (!priv->pipeline)
    return 0.0;

  CLUTTER_GST_NOTE (MEDIA, "get volume: %.02f", priv->volume);

  return priv->volume;
}

static void
clutter_media_init (ClutterMediaIface *iface)
{
}

/*
 * ClutterTexture implementation
 */

static void
clutter_gst_video_texture_size_change (ClutterTexture *texture,
                                       gint            width,
                                       gint            height)
{
  ClutterGstVideoTexture *video_texture = CLUTTER_GST_VIDEO_TEXTURE (texture);
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  gboolean changed;

  /* we are being told the actual (as in number of pixels in the buffers)
   * frame size. Store the values to be used in preferred_width/height() */
  changed = (priv->buffer_width != width) || (priv->buffer_height != height);
  priv->buffer_width = width;
  priv->buffer_height = height;

  if (changed)
    {
      /* reset the computed texture dimensions if the underlying frames have
       * changed size */
      CLUTTER_GST_NOTE (ASPECT_RATIO, "frame size has been updated to %dx%d",
                        width, height);

      priv->texture_width = priv->texture_height = 0;

      /* queue a relayout to ask containers/layout manager to ask for
       * the preferred size again */
      clutter_actor_queue_relayout (CLUTTER_ACTOR (texture));
    }
}

/*
 * Clutter actor implementation
 */

static void
clutter_gst_video_texture_get_natural_size (ClutterGstVideoTexture *texture,
                                            gfloat                 *width,
                                            gfloat                 *height)
{
  ClutterGstVideoTexturePrivate *priv = texture->priv;
  guint dar_n, dar_d;
  gboolean ret;

  /* we cache texture_width and texture_height */

  if (G_UNLIKELY (priv->buffer_width == 0 || priv->buffer_height == 0))
    {
      /* we don't know the size of the frames yet default to 0,0 */
      priv->texture_width = 0;
      priv->texture_height = 0;
    }
  else if (G_UNLIKELY (priv->texture_width == 0 || priv->texture_height == 0))
    {
      CLUTTER_GST_NOTE (ASPECT_RATIO, "frame is %dx%d with par %d/%d",
                        priv->buffer_width, priv->buffer_height,
                        priv->par_n, priv->par_d);

      ret = gst_video_calculate_display_ratio (&dar_n, &dar_d,
                                               priv->buffer_width,
                                               priv->buffer_height,
                                               priv->par_n, priv->par_d,
                                               1, 1);
      if (ret == FALSE)
        dar_n = dar_d = 1;

      if (priv->buffer_height % dar_d == 0)
        {
          priv->texture_width = gst_util_uint64_scale (priv->buffer_height,
                                                       dar_n, dar_d);
          priv->texture_height = priv->buffer_height;
        }
      else if (priv->buffer_width % dar_n == 0)
        {
          priv->texture_width = priv->buffer_width;
          priv->texture_height = gst_util_uint64_scale (priv->buffer_width,
                                                        dar_d, dar_n);

        }
      else
        {
          priv->texture_width = gst_util_uint64_scale (priv->buffer_height,
                                                       dar_n, dar_d);
          priv->texture_height = priv->buffer_height;
        }

      CLUTTER_GST_NOTE (ASPECT_RATIO,
                        "final size is %dx%d (calculated par is %d/%d)",
                        priv->texture_width, priv->texture_height,
                        dar_n, dar_d);
    }

  if (width)
    *width = (gfloat)priv->texture_width;

  if (height)
    *height = (gfloat)priv->texture_height;
}

static void
clutter_gst_video_texture_get_preferred_width (ClutterActor *self,
                                               gfloat        for_height,
                                               gfloat       *min_width_p,
                                               gfloat       *natural_width_p)
{
  ClutterGstVideoTexture *texture = CLUTTER_GST_VIDEO_TEXTURE (self);
  ClutterGstVideoTexturePrivate *priv = texture->priv;
  gboolean sync_size, keep_aspect_ratio;
  gfloat natural_width, natural_height;

  /* Min request is always 0 since we can scale down or clip */
  if (min_width_p)
    *min_width_p = 0;

  sync_size = clutter_texture_get_sync_size (CLUTTER_TEXTURE (self));
  keep_aspect_ratio =
    clutter_texture_get_keep_aspect_ratio (CLUTTER_TEXTURE (self));

  clutter_gst_video_texture_get_natural_size (texture,
                                              &natural_width,
                                              &natural_height);

  if (sync_size)
    {
      if (natural_width_p)
        {
          if (!keep_aspect_ratio ||
              for_height < 0 ||
              priv->buffer_height <= 0)
            {
              *natural_width_p = natural_width;
            }
          else
            {
              /* Set the natural width so as to preserve the aspect ratio */
              gfloat ratio =  natural_width /  natural_height;

              *natural_width_p = ratio * for_height;
            }
        }
    }
  else
    {
      if (natural_width_p)
        *natural_width_p = 0;
    }
}

static void
clutter_gst_video_texture_get_preferred_height (ClutterActor *self,
                                                gfloat        for_width,
                                                gfloat       *min_height_p,
                                                gfloat       *natural_height_p)
{
  ClutterGstVideoTexture *texture = CLUTTER_GST_VIDEO_TEXTURE (self);
  ClutterGstVideoTexturePrivate *priv = texture->priv;
  gboolean sync_size, keep_aspect_ratio;
  gfloat natural_width, natural_height;

  /* Min request is always 0 since we can scale down or clip */
  if (min_height_p)
    *min_height_p = 0;

  sync_size = clutter_texture_get_sync_size (CLUTTER_TEXTURE (self));
  keep_aspect_ratio =
    clutter_texture_get_keep_aspect_ratio (CLUTTER_TEXTURE (self));

  clutter_gst_video_texture_get_natural_size (texture,
                                              &natural_width,
                                              &natural_height);

  if (sync_size)
    {
      if (natural_height_p)
        {
          if (!keep_aspect_ratio ||
              for_width < 0 ||
              priv->buffer_width <= 0)
            {
              *natural_height_p = natural_height;
            }
          else
            {
              /* Set the natural height so as to preserve the aspect ratio */
              gfloat ratio = natural_height / natural_width;

              *natural_height_p = ratio * for_width;
            }
        }
    }
  else
    {
      if (natural_height_p)
        *natural_height_p = 0;
    }
}

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

  /* start by doing the usual clean up when not wanting to play an URI */
  set_uri (self, NULL);

  if (priv->pipeline)
    {
      gst_object_unref (GST_OBJECT (priv->pipeline));
      priv->pipeline = NULL;
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

  free_string_list (&priv->audio_streams);

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

    case PROP_SEEK_FLAGS:
      clutter_gst_video_texture_set_seek_flags (video_texture,
                                                g_value_get_flags (value));
      break;

    case PROP_AUDIO_STREAM:
      clutter_gst_video_texture_set_audio_stream (video_texture,
                                                  g_value_get_int (value));
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

    case PROP_SEEK_FLAGS:
      {
        ClutterGstSeekFlags seek_flags;

        seek_flags = clutter_gst_video_texture_get_seek_flags (video_texture);
        g_value_set_flags (value, seek_flags);
      }
      break;

    case PROP_AUDIO_STREAMS:
      g_value_set_pointer (value, priv->audio_streams);
      break;

    case PROP_AUDIO_STREAM:
      {
        gint index_;

        index_ = clutter_gst_video_texture_get_audio_stream (video_texture);
        g_value_set_int (value, index_);
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
  ClutterTextureClass *texture_class = CLUTTER_TEXTURE_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterGstVideoTexturePrivate));

  object_class->dispose      = clutter_gst_video_texture_dispose;
  object_class->finalize     = clutter_gst_video_texture_finalize;
  object_class->set_property = clutter_gst_video_texture_set_property;
  object_class->get_property = clutter_gst_video_texture_get_property;

  actor_class->paint = clutter_gst_video_texture_paint;
  actor_class->get_preferred_width =
    clutter_gst_video_texture_get_preferred_width;
  actor_class->get_preferred_height =
    clutter_gst_video_texture_get_preferred_height;

  texture_class->size_change = clutter_gst_video_texture_size_change;

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

  pspec = g_param_spec_flags ("seek-flags",
                              "Seek Flags",
                              "Flags to use when seeking",
                              CLUTTER_GST_TYPE_SEEK_FLAGS,
                              CLUTTER_GST_SEEK_FLAG_NONE,
                              CLUTTER_GST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SEEK_FLAGS, pspec);

  pspec = g_param_spec_pointer ("audio-streams",
                                "Audio Streams",
                                "List of the audio streams of the media",
                                CLUTTER_GST_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_AUDIO_STREAMS, pspec);

  pspec = g_param_spec_int ("audio-stream",
                            "Audio Stream",
                            "Index of the current audio stream",
                            -1, G_MAXINT, -1,
                            CLUTTER_GST_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_AUDIO_STREAM, pspec);

  /**
   * ClutterGstVideoTexture::download-buffering:
   * @start: Start of the buffer (between 0.0 and 1.0)
   * @stop: End of the buffer (between 0.0 and 1.0)
   *
   * When streaming, GStreamer can cache the data in a buffer on the disk,
   * something called progressive download or download buffering. This signal
   * is fired when this streaming mode.
   */
  signals[DOWNLOAD_BUFFERING] =
    g_signal_new ("download-buffering",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterGstVideoTextureClass,
                                   download_buffering),
                  NULL, NULL,
                  _clutter_gst_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void
bus_message_error_cb (GstBus                 *bus,
                      GstMessage             *message,
                      ClutterGstVideoTexture *video_texture)
{
  GError *error = NULL;
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  gst_element_set_state(priv->pipeline, GST_STATE_NULL);

  gst_message_parse_error (message, &error, NULL);

  /* restore the idle material so we don't just display the last frame */
  priv->is_idle = TRUE;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (video_texture));

  g_signal_emit_by_name (video_texture, "error", error);

  g_error_free (error);
}

static void
bus_message_eos_cb (GstBus                 *bus,
                    GstMessage             *message,
                    ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;

  CLUTTER_GST_NOTE (MEDIA, "EOS");

  priv->in_eos = TRUE;

  gst_element_set_state(priv->pipeline, GST_STATE_READY);

  /* restore the idle material so we don't just display the last frame */
  priv->is_idle = TRUE;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (video_texture));

  g_signal_emit_by_name (video_texture, "eos");
  g_object_notify (G_OBJECT (video_texture), "progress");
}

static void
bus_message_buffering_cb (GstBus                 *bus,
                          GstMessage             *message,
                          ClutterGstVideoTexture *video_texture)
{
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GstBufferingMode mode;
  GstState current_state;
  gint buffer_percent;

  gst_message_parse_buffering_stats (message, &mode, NULL, NULL, NULL);

  if (mode != GST_BUFFERING_DOWNLOAD)
    priv->in_download_buffering = FALSE;

  switch (mode)
    {
    case GST_BUFFERING_STREAM:
      gst_message_parse_buffering (message, &buffer_percent);
      priv->buffer_fill = CLAMP ((gdouble) buffer_percent / 100.0, 0.0, 1.0);

      CLUTTER_GST_NOTE (BUFFERING, "buffer-fill: %.02f", priv->buffer_fill);

      /* The playbin2 documentation says that we need to pause the pipeline
       * when there's not enough data yet. We try to limit the calls to
       * gst_element_set_state() */
      gst_element_get_state (priv->pipeline, &current_state, NULL, 0);

      if (priv->buffer_fill < 1.0)
        {
          if (current_state != GST_STATE_PAUSED)
            {
              CLUTTER_GST_NOTE (BUFFERING, "pausing the pipeline");
              gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
            }
        }
      else
        {
          if (current_state != priv->target_state)
            {
              CLUTTER_GST_NOTE (BUFFERING, "restoring the pipeline");
              gst_element_set_state (priv->pipeline, priv->target_state);
            }
        }

      g_object_notify (G_OBJECT (video_texture), "buffer-fill");
      break;

    case GST_BUFFERING_DOWNLOAD:
      /* we rate limit the messages from GStreamer for a usage in a UI (we
       * don't want *that* many updates). This is done by installing an idle
       * handler querying the buffer range and sending a signal from there */

      if (priv->in_download_buffering)
        break;

      /* install the querying idle handler the first time we receive a download
       * buffering message */
      configure_buffering_timeout (video_texture, BUFFERING_TIMEOUT);

      /* pause the stream. the idle timeout will set the target state when
       * having received enough data. We'll use buffer_fill as a "virtual
       * stream buffer" to signal the application we're buffering until we
       * can play back from the downloaded stream. */
      gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
      priv->buffer_fill = 0.0;
      g_object_notify (G_OBJECT (video_texture), "buffer-fill");

      priv->download_buffering_element = g_object_ref (message->src);
      priv->in_download_buffering = TRUE;
      priv->virtual_stream_buffer_signalled = 0;
      break;

    case GST_BUFFERING_TIMESHIFT:
    case GST_BUFFERING_LIVE:
    default:
      g_warning ("Buffering mode %d not handled", mode);
      break;
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

  CLUTTER_GST_NOTE (MEDIA, "state change:  %s -> %s",
                    gst_state_to_string (old_state),
                    gst_state_to_string (new_state));

  if (old_state == new_state)
    return;

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
    {
      priv->is_idle = FALSE;
      priv->is_changing_uri = FALSE;
    }

  if (!priv->is_idle)
    {
      if (priv->stacked_progress)
        {
          set_progress (video_texture, priv->stacked_progress);
        }
    }
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
on_volume_changed_main_context (gpointer data)
{
  ClutterGstVideoTexture *video_texture = CLUTTER_GST_VIDEO_TEXTURE (data);
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  gdouble volume;

  volume = gst_stream_volume_get_volume (GST_STREAM_VOLUME (priv->pipeline),
					 GST_STREAM_VOLUME_FORMAT_CUBIC);
  priv->volume = volume;

  g_object_notify (G_OBJECT (video_texture), "audio-volume");

  return FALSE;
}

/* playbin2 proxies the volume property change notification directly from
 * the element having the "volume" property. This means this callback is
 * called from the thread that runs the element, potentially different from
 * the main thread */
static void
on_volume_changed (GstElement             *pipeline,
		   GParamSpec             *pspec,
		   ClutterGstVideoTexture *video_texture)
{
  g_idle_add (on_volume_changed_main_context, video_texture);
}

static GList *
get_tags (GstElement  *pipeline,
          const gchar *property_name,
          const gchar *action_signal)
{
  GList *ret = NULL;
  gint num = 1, i, n;

  g_object_get (G_OBJECT (pipeline), property_name, &n, NULL);
  if (n == 0)
    return NULL;

  for (i = 0; i < n; i++)
    {
      GstTagList *tags = NULL;
      gchar *description = NULL;

      g_signal_emit_by_name (G_OBJECT (pipeline), action_signal, i, &tags);

      if (tags)
        {

          gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &description);

          if (description)
            {
              const gchar *language = gst_tag_get_language_name (description);

              if (language)
                {
                  g_free (description);
                  description = g_strdup (language);
                }
            }

          if (!description)
            gst_tag_list_get_string (tags, GST_TAG_CODEC, &description);

          gst_tag_list_free (tags);
        }

      if (!description)
        description = g_strdup_printf ("Audio Track #%d", num++);

      ret = g_list_prepend (ret, description);

    }

  return g_list_reverse (ret);
}

static gboolean
are_lists_equal (GList *list1,
                 GList *list2)
{
  GList *l1, *l2;

  l1 = list1;
  l2 = list2;

  while (l1)
    {
      const gchar *str1, *str2;

      if (l2 == NULL)
        return FALSE;

      str1 = l1->data;
      str2 = l2->data;

      if (g_strcmp0 (str1, str2) != 0)
        return FALSE;

      l1 = g_list_next (l1);
      l2 = g_list_next (l2);
    }

  return l2 == NULL;
}

static gboolean
on_audio_changed_main_context (gpointer data)
{
  ClutterGstVideoTexture *video_texture = CLUTTER_GST_VIDEO_TEXTURE (data);
  ClutterGstVideoTexturePrivate *priv = video_texture->priv;
  GList *audio_streams;

  audio_streams = get_tags (priv->pipeline, "n-audio", "get-audio-tags");

  if (!are_lists_equal (priv->audio_streams, audio_streams))
    {
      free_string_list (&priv->audio_streams);
      priv->audio_streams = audio_streams;

      CLUTTER_GST_NOTE (AUDIO_STREAM, "audio-streams changed");

      g_object_notify (G_OBJECT (video_texture), "audio-streams");
    }
  else
    {
      free_string_list (&audio_streams);
    }

  return FALSE;
}

/* same explanation as for notify::volume's usage of g_idle_add() */
static void
on_audio_changed (GstElement             *pipeline,
                  ClutterGstVideoTexture *video_texture)
{
  g_idle_add (on_audio_changed_main_context, video_texture);
}

static void
on_audio_tags_changed (GstElement             *pipeline,
                       gint                    stream,
                       ClutterGstVideoTexture *video_texture)
{
  g_idle_add (on_audio_changed_main_context, video_texture);
}

static gboolean
on_current_audio_changed_main_context (gpointer data)
{
  ClutterGstVideoTexture *video_texture = CLUTTER_GST_VIDEO_TEXTURE (data);

  CLUTTER_GST_NOTE (AUDIO_STREAM, "audio stream changed");
  g_object_notify (G_OBJECT (video_texture), "audio-stream");

  return FALSE;
}

static void
on_current_audio_changed (GstElement             *pipeline,
                          GParamSpec             *pspec,
                          ClutterGstVideoTexture *video_texture)
{
  g_idle_add (on_current_audio_changed_main_context, video_texture);
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
  priv->is_changing_uri = FALSE;
  priv->in_download_buffering = FALSE;

  priv->par_n = priv->par_d = 1;

  /* We default to not playing until someone calls set_playing(TRUE) */
  priv->target_state = GST_STATE_PAUSED;

  /* Default to a fast seek, ie. same effect than set_seek_flags (NONE); */
  priv->seek_flags = GST_SEEK_FLAG_KEY_UNIT;

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

  g_signal_connect (priv->pipeline, "notify::volume",
		    G_CALLBACK (on_volume_changed), video_texture);

  g_signal_connect (priv->pipeline, "audio-changed",
                    G_CALLBACK (on_audio_changed), video_texture);
  g_signal_connect (priv->pipeline, "audio-tags-changed",
                    G_CALLBACK (on_audio_tags_changed), video_texture);
  g_signal_connect (priv->pipeline, "notify::current-audio",
                    G_CALLBACK (on_current_audio_changed), video_texture);

  gst_object_unref (GST_OBJECT (bus));
}

/*
 * Private symbols
 */

/* This function is called from the sink set_caps(). we receive the first
 * buffer way after this so are told about the par before size_changed has
 * been fired */
void
_clutter_gst_video_texture_set_par (ClutterGstVideoTexture *texture,
                                    guint                   par_n,
                                    guint                   par_d)
{
  ClutterGstVideoTexturePrivate *priv = texture->priv;

  priv->par_n = par_n;
  priv->par_d = par_d;
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
 * Return value: (transfer none): the pipeline element used by the video texture
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
 * Return value: (transfer none): the #CoglHandle of the idle material
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

/**
 * clutter_gst_video_texture_get_seek_flags:
 * @texture: a #ClutterGstVideoTexture
 *
 * Get the current value of the seek-flags property.
 *
 * Return value: a combination of #ClutterGstSeekFlags
 *
 * Since: 1.4
 */
ClutterGstSeekFlags
clutter_gst_video_texture_get_seek_flags (ClutterGstVideoTexture *texture)
{
  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture),
                        CLUTTER_GST_SEEK_FLAG_NONE);

  if (texture->priv->seek_flags == GST_SEEK_FLAG_ACCURATE)
    return CLUTTER_GST_SEEK_FLAG_ACCURATE;
  else
    return CLUTTER_GST_SEEK_FLAG_NONE;
}

/**
 * clutter_gst_video_texture_set_seek_flags:
 * @texture: a #ClutterGstVideoTexture
 * @flags: a combination of #ClutterGstSeekFlags
 *
 * Seeking can be done with several trade-offs. Clutter-gst defaults
 * to %CLUTTER_GST_SEEK_FLAG_NONE.
 *
 * Since: 1.4
 */
void
clutter_gst_video_texture_set_seek_flags (ClutterGstVideoTexture *texture,
                                          ClutterGstSeekFlags     flags)
{
  ClutterGstVideoTexturePrivate *priv;

  g_return_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture));
  priv = texture->priv;

  if (flags == CLUTTER_GST_SEEK_FLAG_NONE)
    priv->seek_flags = GST_SEEK_FLAG_KEY_UNIT;
  else if (flags & CLUTTER_GST_SEEK_FLAG_ACCURATE)
    priv->seek_flags = GST_SEEK_FLAG_ACCURATE;
}

/**
 * clutter_gst_video_texture_get_buffering_mode:
 * @texture: a #ClutterGstVideoTexture
 *
 * Return value: a #ClutterGstBufferingMode
 *
 * Since: 1.4
 */
ClutterGstBufferingMode
clutter_gst_video_texture_get_buffering_mode (ClutterGstVideoTexture *texture)
{
  ClutterGstVideoTexturePrivate *priv;
  GstPlayFlags flags;

  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture),
                        CLUTTER_GST_BUFFERING_MODE_STREAM);
  priv = texture->priv;

  g_object_get (G_OBJECT (priv->pipeline), "flags", &flags, NULL);
  if (flags & GST_PLAY_FLAG_DOWNLOAD)
    return CLUTTER_GST_BUFFERING_MODE_DOWNLOAD;

  return CLUTTER_GST_BUFFERING_MODE_STREAM;
}

/**
 * clutter_gst_video_texture_set_buffering_mode:
 * @texture: a #ClutterGstVideoTexture
 * @mode: a #ClutterGstBufferingMode
 *
 * Since: 1.4
 */
void
clutter_gst_video_texture_set_buffering_mode (ClutterGstVideoTexture *texture,
                                              ClutterGstBufferingMode mode)
{
  ClutterGstVideoTexturePrivate *priv;
  GstPlayFlags flags;

  g_return_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture));
  priv = texture->priv;

  g_object_get (G_OBJECT (priv->pipeline), "flags", &flags, NULL);

  switch (mode)
    {
    case CLUTTER_GST_BUFFERING_MODE_STREAM:
      flags &= ~GST_PLAY_FLAG_DOWNLOAD;
      break;
    case CLUTTER_GST_BUFFERING_MODE_DOWNLOAD:
      flags |= GST_PLAY_FLAG_DOWNLOAD;
      break;
    default:
      g_warning ("Unexpected buffering mode %d", mode);
      break;
    }

  g_object_set (G_OBJECT (priv->pipeline), "flags", flags, NULL);
}

#ifdef CLUTTER_GST_ENABLE_DEBUG
gchar *
list_to_string (GList *list)
{
  GString *string;
  GList *l;
  gint n, i;

  if (!list)
    return g_strdup ("<empty list>");

  string = g_string_new (NULL);
  n = g_list_length (list);
  for (i = 0, l = list; i < n - 1; i++, l = g_list_next (l))
    g_string_append_printf (string, "%s, ", (gchar *) l->data);

  g_string_append_printf (string, "%s", (gchar *) l->data);


  return g_string_free (string, FALSE);
}
#endif

/**
 * clutter_gst_video_texture_get_audio_streams:
 * @texture: a #ClutterGstVideoTexture
 *
 * Get the list of audio streams of the current media.
 *
 * Return value: a list of strings describing the available audio streams
 *
 * Since: 1.4
 */
GList *
clutter_gst_video_texture_get_audio_streams (ClutterGstVideoTexture *texture)
{
  ClutterGstVideoTexturePrivate *priv = texture->priv;

  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture), NULL);

  if (CLUTTER_GST_DEBUG_ENABLED (AUDIO_STREAM))
    {
      gchar *streams;

      streams = list_to_string (priv->audio_streams);
      CLUTTER_GST_NOTE (AUDIO_STREAM, "audio streams: %s", streams);
      g_free (streams);
    }

  return priv->audio_streams;
}

/**
 * clutter_gst_video_texture_get_audio_stream:
 * @texture: a #ClutterGstVideoTexture
 *
 * Get the current audio stream. The number returned in the index of the
 * audio stream playing in the list returned by
 * clutter_gst_video_texture_get_audio_streams().
 *
 * Return value: the index of the current audio stream, -1 if the media has no
 * audio stream
 *
 * Since: 1.4
 */
gint
clutter_gst_video_texture_get_audio_stream (ClutterGstVideoTexture *texture)
{
  gint index_ = -1;

  g_return_val_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture), -1);

  g_object_get (G_OBJECT (texture->priv->pipeline),
                "current-audio", &index_,
                NULL);

  CLUTTER_GST_NOTE (AUDIO_STREAM, "audio stream is #%d", index_);

  return index_;
}

/**
 * clutter_gst_video_texture_set_audio_stream:
 * @texture: a #ClutterGstVideoTexture
 * @index_: the index of the audio stream
 *
 * Set the audio stream to play. @index_ is the index of the stream
 * in the list returned by clutter_gst_video_texture_get_audio_streams().
 *
 * Since: 1.4
 */
void
clutter_gst_video_texture_set_audio_stream (ClutterGstVideoTexture *texture,
                                            gint                    index_)
{
  ClutterGstVideoTexturePrivate *priv = texture->priv;

  g_return_if_fail (CLUTTER_GST_IS_VIDEO_TEXTURE (texture));
  g_return_if_fail (index_ >= 0 &&
                    index_ < g_list_length (priv->audio_streams));

  CLUTTER_GST_NOTE (AUDIO_STREAM, "set audio audio stream to #%d", index_);

  g_object_set (G_OBJECT (priv->pipeline), "current-audio", index_, NULL);
}
