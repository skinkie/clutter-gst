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
 * SECTION:clutter-video-sink
 * @short_description: GStreamer video sink
 *
 * #ClutterGstVideoSink is a GStreamer sink element that sends
 * data to a #ClutterTexture
 */

#include "config.h"

#include "clutter-gst-video-sink.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <glib.h>
#include <clutter/clutter.h>
#include <string.h>

static GstStaticPadTemplate sinktemplate 
 = GST_STATIC_PAD_TEMPLATE ("sink",
			    GST_PAD_SINK,
			    GST_PAD_ALWAYS,
			    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBx ";"   \
                                             GST_VIDEO_CAPS_BGRx  ) \
			    );

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

struct _ClutterGstVideoSinkPrivate
{
  ClutterTexture *texture;
  guint           bus_id;
  GstBuffer      *scratch_buffer;
  GMutex         *scratch_lock;
  gboolean        rgb_ordering;
  int             width;
  int             height;
  int             fps_n, fps_d;
  int             par_n, par_d;
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

  gst_element_class_add_pad_template 
                     (element_class,
		      gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_details (element_class, 
				 &clutter_gst_video_sink_details);
}

static void
clutter_gst_video_sink_init (ClutterGstVideoSink *sink,
    			     ClutterGstVideoSinkClass *klass)
{
  ClutterGstVideoSinkPrivate *priv;

  priv = g_new0 (ClutterGstVideoSinkPrivate, 1);
  sink->priv = priv;

  sink->priv->scratch_lock = g_mutex_new ();
}

static void
clutter_gst_video_sink_bus_cb (GstBus              *bus,
    			       GstMessage          *message,
			       ClutterGstVideoSink *sink)
{
  ClutterGstVideoSinkPrivate *priv;

  if (sink->priv->texture == NULL)
    return;

  priv = sink->priv;

  g_mutex_lock (priv->scratch_lock);

  if (priv->scratch_buffer != NULL)
    {
      clutter_texture_set_from_rgb_data 
	(sink->priv->texture,
		    GST_BUFFER_DATA (sink->priv->scratch_buffer),
		    TRUE,
		    sink->priv->width,
		    sink->priv->height,
		    (4 * sink->priv->width + 3) &~ 3,
		    4,
		    sink->priv->rgb_ordering ? 
	                   0 : CLUTTER_TEXTURE_RGB_FLAG_BGR,
		    NULL);

      gst_buffer_unref (sink->priv->scratch_buffer);
      sink->priv->scratch_buffer = NULL;
    }

  g_mutex_unlock (sink->priv->scratch_lock);
}

static GstFlowReturn
clutter_gst_video_sink_render (GstBaseSink *bsink, GstBuffer *buffer)
{
  ClutterGstVideoSink *sink;
  GstMessage *msg;

  sink = CLUTTER_GST_VIDEO_SINK (bsink);

  if (sink->priv->bus_id == 0)
    {
      GstBus *bus;
      GstElement *lp = NULL;
      GstElement *p = GST_ELEMENT (sink);

      /* find the outermost element and use its bus */
      while (p != NULL) {
	lp = p;
	p = GST_ELEMENT_PARENT (lp);
      }

      bus = GST_ELEMENT_BUS (lp);
      sink->priv->bus_id =
	g_signal_connect_object (bus,
				 "message::element",
				 G_CALLBACK (clutter_gst_video_sink_bus_cb),
				 sink,
				 0);
    }

  g_mutex_lock (sink->priv->scratch_lock);

  if (sink->priv->scratch_buffer)
    gst_buffer_unref (sink->priv->scratch_buffer);
  sink->priv->scratch_buffer = gst_buffer_ref (buffer);

  msg = gst_message_new_element (GST_OBJECT (sink), NULL);
  gst_element_post_message (GST_ELEMENT (sink), msg);

  g_mutex_unlock (sink->priv->scratch_lock);

  return GST_FLOW_OK;
}

static gboolean
clutter_gst_video_sink_set_caps (GstBaseSink *bsink, GstCaps *caps)
{
  ClutterGstVideoSink        *sink;
  ClutterGstVideoSinkPrivate *priv;
  GstCaps                    *intersection;
  GstStructure               *structure;
  gboolean                    ret;
  const GValue               *fps;
  const GValue               *par;
  gint                        width, height;   
  int                         red_mask;

  sink = CLUTTER_GST_VIDEO_SINK(bsink);
  priv = sink->priv;

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

  gst_structure_get_int (structure, "red_mask", &red_mask);
  priv->rgb_ordering = (red_mask == 0xff000000);

  return TRUE;
}

static void
clutter_gst_video_sink_dispose (GObject *object)
{
  ClutterGstVideoSink *self;

  self = CLUTTER_GST_VIDEO_SINK(object);

  g_mutex_lock (self->priv->scratch_lock);
  if (self->priv->scratch_buffer)
    {
      gst_buffer_unref (self->priv->scratch_buffer);
      self->priv->scratch_buffer = NULL;
    }
  g_mutex_unlock (self->priv->scratch_lock);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clutter_gst_video_sink_finalize (GObject *object)
{
  ClutterGstVideoSink *self;

  self = CLUTTER_GST_VIDEO_SINK(object);

  g_mutex_free (self->priv->scratch_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clutter_gst_video_sink_set_property (GObject *object,
    				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
  ClutterGstVideoSink *sink;

  sink = CLUTTER_GST_VIDEO_SINK (object);

  switch (prop_id) 
    {
    case PROP_TEXTURE:
      if (sink->priv->texture != NULL)
	{
	  g_object_unref (sink->priv->texture);
	}
      sink->priv->texture = g_value_get_object (value);
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

static void
clutter_gst_video_sink_class_init (ClutterGstVideoSinkClass *klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbase_sink_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbase_sink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->set_property = clutter_gst_video_sink_set_property;
  gobject_class->get_property = clutter_gst_video_sink_get_property;

  gobject_class->dispose     = clutter_gst_video_sink_dispose;
  gobject_class->finalize    = clutter_gst_video_sink_finalize;

  gstbase_sink_class->render  = clutter_gst_video_sink_render;
  gstbase_sink_class->preroll = clutter_gst_video_sink_render;
  gstbase_sink_class->set_caps = clutter_gst_video_sink_set_caps;

  g_object_class_install_property 
              (gobject_class, PROP_TEXTURE,
	       g_param_spec_object ("texture",
				    "texture",
				    "Target ClutterTexture object",
				    CLUTTER_TYPE_TEXTURE,
				    G_PARAM_READWRITE));
}

GstElement *
clutter_gst_video_sink_new (ClutterTexture *texture)
{
  GstElement *element;

  element = GST_ELEMENT (g_object_new (CLUTTER_GST_TYPE_VIDEO_SINK,
				       "texture", texture,
				       NULL));
  return element;
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
			  "element to render to clutter textures",
			  plugin_init,
			  VERSION,
			  "LGPL",
			  PACKAGE,
			  "");

