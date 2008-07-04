/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-video-sink.h - Gstreamer Video Sink that renders to a
 *                            Clutter Texture.
 *
 * Authored by Jonathan Matthew  <jonathan@kaolin.wh9.net>
 *
 * Copyright (C) 2007 OpenedHand
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
 * SECTION:clutter-gst-video-sink
 * @short_description: GStreamer video sink
 *
 * #ClutterGstVideoSink is a GStreamer sink element that sends
 * data to a #ClutterTexture.
 */

#include "config.h"

#include "clutter-gst-video-sink.h"
#include "clutter-gst-shaders.h"

#include <gst/gst.h>
#include <gst/gstvalue.h>
#include <gst/video/video.h>
#include <gst/riff/riff-ids.h>

#include <glib.h>
#include <clutter/clutter.h>
#include <string.h>

static gchar *ayuv_to_rgba_shader = \
     FRAGMENT_SHADER_VARS
     FRAGMENT_SHADER_BEGIN
     "  color.bgra = vec4((1.164383 * (color.g - 0.0625)) +         "
     "                    (1.596027 * (color.b - 0.5)),             "
     "                    (1.164383 * (color.g - 0.0625)) -         "
     "                    (0.812968 * (color.a - 0.5)) -            "
     "                    (0.391762 * (color.b - 0.5)),             "
     "                    (1.164383 * (color.g - 0.0625)) -         "
     "                    (2.017232 * (color.b - 0.5)),             "
     "                    color.r);                                 "
     FRAGMENT_SHADER_END;

static gchar *yv12_to_rgba_shader = \
     FRAGMENT_SHADER_VARS
     /* FIXME: Need to sample four pixels with get_uv. Really, we want to use
      *        multi-texturing + 8-bit textures here, that would massively
      *        simplify this and un-break interpolation.
      */
     "float get_uv (int x, int y, bool v)"
     "{"
     "  int iwidth = int (width);"
     "  int iheight = int (height);"
     "  int stride = iwidth * 3;"
     "  int idx = (y * iwidth/2) + x + (iheight * iwidth);"
     "  if (v) idx += iheight/2 * iwidth/2;"
     "  int yt = idx / stride;"
     "  int ym = idx % stride;"
     "  int xt = ym / 3;"
     "  int xm = ym % 3;"
     "  float s = (float (xt)+0.5) / width;"
     "  float t = (float (yt)+0.5) / height;"
     "  vec4 pix = texture2D (tex, vec2(s,t));"
     "  float uvc = (xm == 0) ? pix.r : ((xm == 1) ? pix.g : pix.b);"
     "  return uvc;"
     "}"

     "float get_y (int x, int y)"
     "{"
     "  int iwidth = int (width);"
     "  int stride = iwidth * 3;"
     "  int idx = (y * iwidth) + x;"
     "  int yt = idx / stride;"
     "  int ym = idx % stride;"
     "  int xt = ym / 3;"
     "  int xm = ym % 3;"
     "  float s = (float (xt)+0.5) / width;"
     "  float t = (float (yt)+0.5) / height;"
     "  vec4 pix = texture2D (tex, vec2(s,t));"
     "  float yc = (xm == 0) ? pix.r : ((xm == 1) ? pix.g : pix.b);"
     "  return yc;"
     "}"
     
     FRAGMENT_SHADER_BEGIN
     "  float s = " TEX_COORD ".s * width;                         "
     "  float t = " TEX_COORD ".t * height;                         "
     "  int is = int (s);"
     "  int it = int (t);"
     "  float y = get_y (is, it);"
     "  is /= 2;"
     "  it /= 2;"
     "  float u = get_uv (is, it, false);"
     "  float v = get_uv (is, it, true);"
     
     "  color.rgba = vec4((1.164383 * (y - 0.0625)) +         "
     "                    (1.596027 * (u - 0.5)),             "
     "                    (1.164383 * (y - 0.0625)) -         "
     "                    (0.812968 * (v - 0.5)) -            "
     "                    (0.391762 * (u - 0.5)),             "
     "                    (1.164383 * (y - 0.0625)) -         "
     "                    (2.017232 * (u - 0.5)),             "
     "                    1.0);                                 "
     FRAGMENT_SHADER_END;

static GstStaticPadTemplate sinktemplate 
 = GST_STATIC_PAD_TEMPLATE ("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBx ";"   \
                                             GST_VIDEO_CAPS_BGRx));

/* Define USE_YV12_SHADER to use experimental YV12 decoding shader */
#ifdef USE_YV12_SHADER
#define YV12_CAPS ";" GST_VIDEO_CAPS_YUV("YV12")
#else
#define YV12_CAPS
#endif

static GstStaticPadTemplate sinktemplate_shaders 
 = GST_STATIC_PAD_TEMPLATE ("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV("AYUV") ";" \
                                             GST_VIDEO_CAPS_RGBx ";"   \
                                             GST_VIDEO_CAPS_BGRx \
                                             YV12_CAPS \
                                             ));

GST_DEBUG_CATEGORY_STATIC (clutter_gst_video_sink_debug);
#define GST_CAT_DEFAULT clutter_gst_video_sink_debug

static GstElementDetails clutter_gst_video_sink_details =
  GST_ELEMENT_DETAILS ("Clutter video sink",
      "Sink/Video",
      "Sends video data from a GStreamer pipeline to a Clutter texture",
      "Jonathan Matthew <jonathan@kaolin.wh9.net>, Matthew Allum <mallum@o-hand.com");

enum
{
  PROP_0,
  PROP_TEXTURE
};

typedef enum
{
  CLUTTER_GST_RGB,
  CLUTTER_GST_BGR,
  CLUTTER_GST_YV12,
} ClutterGstVideoFormat;

struct _ClutterGstVideoSinkPrivate
{
  ClutterTexture        *texture;
  GAsyncQueue           *async_queue;
  ClutterGstVideoFormat  format;
  int                    width;
  int                    height;
  int                    fps_n, fps_d;
  int                    par_n, par_d;
  gboolean               first_frame;
};


#define _do_init(bla) \
  GST_DEBUG_CATEGORY_INIT (clutter_gst_video_sink_debug, \
                                 "cluttersink", \
                                 0, \
                                 "clutter video sink")

GST_BOILERPLATE_FULL (ClutterGstVideoSink,
                          clutter_gst_video_sink,
                      GstBaseSink,
                      GST_TYPE_BASE_SINK,
                      _do_init);

static void
clutter_gst_video_sink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  if (cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    gst_element_class_add_pad_template 
                       (element_class,
                        gst_static_pad_template_get (&sinktemplate_shaders));
  else
    gst_element_class_add_pad_template 
                       (element_class,
                        gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_details (element_class, 
                                 &clutter_gst_video_sink_details);
}

static void
clutter_gst_video_sink_init (ClutterGstVideoSink      *sink,
                             ClutterGstVideoSinkClass *klass)
{
  ClutterGstVideoSinkPrivate *priv;

  sink->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (sink, CLUTTER_GST_TYPE_VIDEO_SINK,
                                 ClutterGstVideoSinkPrivate);

  priv->async_queue = g_async_queue_new ();
}

static gboolean
clutter_gst_video_sink_idle_func (gpointer data)
{
  ClutterGstVideoSinkPrivate *priv;
  GstBuffer *buffer;

  priv = data;

  buffer = g_async_queue_try_pop (priv->async_queue);
  if (buffer == NULL || G_UNLIKELY (!GST_IS_BUFFER (buffer)))
    {
      return FALSE;
    }

  if ((priv->format == CLUTTER_GST_RGB) ||
      (priv->format == CLUTTER_GST_BGR))
    {
      clutter_texture_set_from_rgb_data (priv->texture,
                                         GST_BUFFER_DATA (buffer),
                                         TRUE,
                                         priv->width,
                                         priv->height,
                                         GST_ROUND_UP_4 (4 * priv->width),
                                         4,
                                         (priv->format == CLUTTER_GST_RGB) ?
                                         0 : CLUTTER_TEXTURE_RGB_FLAG_BGR,
                                         NULL);
    }
  else if (priv->format == CLUTTER_GST_YV12)
    {
      if (priv->first_frame)
        {
          guchar        *pixels = g_malloc (GST_ROUND_UP_4 (priv->width * priv->height * 3));
          clutter_texture_set_from_rgb_data (priv->texture,
                                             pixels,
                                             FALSE,
                                             priv->width,
                                             priv->height,
                                             priv->width * 3,
                                             3,
                                             0,
                                             NULL);
          g_free (pixels);
          priv->first_frame = FALSE;
        }

      clutter_texture_set_area_from_rgb_data (priv->texture,
                                              GST_BUFFER_DATA (buffer),
                                              FALSE,
                                              0,
                                              0,
                                              priv->width,
                                              priv->height / 2,
                                              priv->width * 3,
                                              3,
                                              0,
                                              NULL);
    }

  gst_buffer_unref (buffer);

  return FALSE;
}

static GstFlowReturn
clutter_gst_video_sink_render (GstBaseSink *bsink,
                               GstBuffer   *buffer)
{
  ClutterGstVideoSink *sink;
  ClutterGstVideoSinkPrivate *priv;

  sink = CLUTTER_GST_VIDEO_SINK (bsink);
  priv = sink->priv;

  g_async_queue_push (priv->async_queue, gst_buffer_ref (buffer));

  clutter_threads_add_idle_full (G_PRIORITY_HIGH_IDLE,
                                 clutter_gst_video_sink_idle_func,
                                 priv,
                                 NULL);

  return GST_FLOW_OK;
}

static gboolean
clutter_gst_video_sink_set_caps (GstBaseSink *bsink,
                                 GstCaps     *caps)
{
  ClutterGstVideoSink        *sink;
  ClutterGstVideoSinkPrivate *priv;
  GstCaps                    *intersection;
  GstStructure               *structure;
  gboolean                    ret;
  const GValue               *fps;
  const GValue               *par;
  gint                        width, height;
  guint32                     fourcc;
  int                         red_mask;

  sink = CLUTTER_GST_VIDEO_SINK(bsink);
  priv = sink->priv;

  if (cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    intersection 
      = gst_caps_intersect 
            (gst_static_pad_template_get_caps (&sinktemplate_shaders), 
             caps);
  else
    intersection 
      = gst_caps_intersect 
            (gst_static_pad_template_get_caps (&sinktemplate), 
             caps);

  if (gst_caps_is_empty (intersection)) 
    return FALSE;

  gst_caps_unref (intersection);

  structure = gst_caps_get_structure (caps, 0);

  ret  = gst_structure_get_int (structure, "width", &width);
  ret &= gst_structure_get_int (structure, "height", &height);
  fps  = gst_structure_get_value (structure, "framerate");
  ret &= (fps != NULL);

  par  = gst_structure_get_value (structure, "pixel-aspect-ratio");

  if (!ret)
    return FALSE;

  priv->width  = width;
  priv->height = height;
  priv->first_frame = TRUE;

  /* We dont yet use fps or pixel aspect into but handy to have */
  priv->fps_n  = gst_value_get_fraction_numerator (fps);
  priv->fps_d  = gst_value_get_fraction_denominator (fps);

  if (par) 
    {
      priv->par_n = gst_value_get_fraction_numerator (par);
      priv->par_d = gst_value_get_fraction_denominator (par);
    } 
  else 
    priv->par_n = priv->par_d = 1;

  ret = gst_structure_get_fourcc (structure, "format", &fourcc);
  if (ret && (fourcc == GST_RIFF_YV12))
    {
      ClutterShader *shader;
      ClutterActor  *actor = CLUTTER_ACTOR (priv->texture);
      
      shader = clutter_shader_new ();
      clutter_shader_set_fragment_source (shader, yv12_to_rgba_shader, -1);
      clutter_actor_set_shader (actor, shader);
      clutter_actor_set_shader_param (actor, "width", priv->width);
      clutter_actor_set_shader_param (actor, "height", priv->height);
      g_object_unref (shader);
      priv->format = CLUTTER_GST_YV12;
    }
  else if (ret && (fourcc == GST_MAKE_FOURCC ('A', 'Y', 'U', 'V')))
    {
      ClutterShader *shader;
      
      shader = clutter_shader_new ();
      clutter_shader_set_fragment_source (shader, ayuv_to_rgba_shader, -1);
      clutter_actor_set_shader (CLUTTER_ACTOR (priv->texture), shader);
      priv->format = CLUTTER_GST_RGB;
      g_object_unref (shader);
    }
  else
    {
      gst_structure_get_int (structure, "red_mask", &red_mask);
      priv->format = (red_mask == 0xff000000) ?
        CLUTTER_GST_RGB : CLUTTER_GST_BGR;
    }

  return TRUE;
}

static void
clutter_gst_video_sink_dispose (GObject *object)
{
  ClutterGstVideoSink *self;
  ClutterGstVideoSinkPrivate *priv;

  self = CLUTTER_GST_VIDEO_SINK (object);
  priv = self->priv;

  if (priv->texture)
    {
      g_object_unref (priv->texture);
      priv->texture = NULL;
    }

  if (priv->async_queue)
    {
      g_async_queue_unref (priv->async_queue);
      priv->async_queue = NULL;
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clutter_gst_video_sink_finalize (GObject *object)
{
  ClutterGstVideoSink *self;
  ClutterGstVideoSinkPrivate *priv;

  self = CLUTTER_GST_VIDEO_SINK (object);
  priv = self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clutter_gst_video_sink_set_property (GObject *object,
                                         guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
  ClutterGstVideoSink *sink;
  ClutterGstVideoSinkPrivate *priv;

  sink = CLUTTER_GST_VIDEO_SINK (object);
  priv = sink->priv;

  switch (prop_id) 
    {
    case PROP_TEXTURE:
      if (priv->texture)
        g_object_unref (priv->texture);

      priv->texture = CLUTTER_TEXTURE (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_gst_video_sink_get_property (GObject *object,
                                         guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
  ClutterGstVideoSink *sink;

  sink = CLUTTER_GST_VIDEO_SINK (object);

  switch (prop_id) 
    {
    case PROP_TEXTURE:
      g_value_set_object (value, sink->priv->texture);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
clutter_gst_video_sink_stop (GstBaseSink *base_sink)
{
  ClutterGstVideoSinkPrivate *priv;
  GstBuffer *buffer;

  priv = CLUTTER_GST_VIDEO_SINK (base_sink)->priv;

  g_async_queue_lock (priv->async_queue);

  /* Remove all remaining objects from the queue */
  do
    {
      buffer = g_async_queue_try_pop_unlocked (priv->async_queue);
      if (buffer)
        gst_buffer_unref (buffer);
    } while (buffer != NULL);

  g_async_queue_unlock (priv->async_queue);

  return TRUE;
}

static void
clutter_gst_video_sink_class_init (ClutterGstVideoSinkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbase_sink_class = GST_BASE_SINK_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterGstVideoSinkPrivate));

  gobject_class->set_property = clutter_gst_video_sink_set_property;
  gobject_class->get_property = clutter_gst_video_sink_get_property;

  gobject_class->dispose = clutter_gst_video_sink_dispose;
  gobject_class->finalize = clutter_gst_video_sink_finalize;

  gstbase_sink_class->render = clutter_gst_video_sink_render;
  gstbase_sink_class->preroll = clutter_gst_video_sink_render;
  gstbase_sink_class->stop = clutter_gst_video_sink_stop;
  gstbase_sink_class->set_caps = clutter_gst_video_sink_set_caps;

  g_object_class_install_property 
              (gobject_class, PROP_TEXTURE,
               g_param_spec_object ("texture",
                                    "texture",
                                    "Target ClutterTexture object",
                                    CLUTTER_TYPE_TEXTURE,
                                    G_PARAM_READWRITE));
}

/**
 * clutter_gst_video_sink_new:
 * @texture: a #ClutterTexture
 *
 * Creates a new GStreamer video sink which uses @texture as the target
 * for sinking a video stream from GStreamer.
 *
 * Return value: a #GstElement for the newly created video sink
 */
GstElement *
clutter_gst_video_sink_new (ClutterTexture *texture)
{
  return g_object_new (CLUTTER_GST_TYPE_VIDEO_SINK,
                       "texture", texture,
                       NULL);
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  gboolean ret = gst_element_register (plugin,
                                             "cluttersink",
                                       GST_RANK_PRIMARY,
                                       CLUTTER_GST_TYPE_VIDEO_SINK);
  return ret;
}

GST_PLUGIN_DEFINE_STATIC (GST_VERSION_MAJOR,
                          GST_VERSION_MINOR,
                          "cluttersink",
                          "Element to render to Clutter textures",
                          plugin_init,
                          VERSION,
                          "LGPL", /* license */
                          PACKAGE,
                          "");
