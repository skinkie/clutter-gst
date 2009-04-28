#include <string.h>

#include <clutter-gst/clutter-gst.h>

static gint   opt_framerate = 25;
static gint   opt_bpp       = 24;
static gint   opt_depth     = 24;

static GOptionEntry options[] =
{
  { "framerate",
    'f', 0,
    G_OPTION_ARG_INT,
    &opt_framerate,
    "Number of frames per second (default is 25)",
    NULL
  },
  { "bpp",
    'b', 0,
    G_OPTION_ARG_INT,
    &opt_bpp,
    "bits per pixel (default is 32)",
    NULL
  },
  { "depth",
    'd', 0,
    G_OPTION_ARG_INT,
    &opt_depth,
    "depth (default is 24)",
    NULL
  },

  { NULL }
};

void
size_change (ClutterTexture *texture,
	     gint            width,
	     gint            height,
	     gpointer        user_data)
{
  gint           new_x, new_y, new_width, new_height;

  new_height = ( height * CLUTTER_STAGE_WIDTH() ) / width;
  if (new_height <= CLUTTER_STAGE_HEIGHT())
    {
      new_width = CLUTTER_STAGE_WIDTH();

      new_x = 0;
      new_y = (CLUTTER_STAGE_HEIGHT() - new_height) / 2;
    }
  else
    {
      new_width  = ( width * CLUTTER_STAGE_HEIGHT() ) / height;
      new_height = CLUTTER_STAGE_HEIGHT();

      new_x = (CLUTTER_STAGE_WIDTH() - new_width) / 2;
      new_y = 0;
    }

  clutter_actor_set_position (CLUTTER_ACTOR (texture), new_x, new_y);

  clutter_actor_set_size (CLUTTER_ACTOR (texture),
			  new_width,
			  new_height);
}

int
main (int argc, char *argv[])
{
  GOptionContext   *context;
  gboolean          result;
  ClutterActor     *stage;
  ClutterActor     *texture;
  GstPipeline      *pipeline;
  GstElement       *src;
  GstElement       *capsfilter;
  GstElement       *colorspace;
  GstElement       *sink;
  GstCaps          *caps;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  context = g_option_context_new (" - test-colorspace options");
  g_option_context_add_group (context, gst_init_get_option_group ());
  g_option_context_add_group (context, clutter_get_option_group ());
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, NULL);

  stage = clutter_stage_get_default ();

  /* We need to set certain props on the target texture currently for
   * efficient/corrent playback onto the texture (which sucks a bit)
  */
  texture = g_object_new (CLUTTER_TYPE_TEXTURE,
                          "sync-size",       FALSE,
                          "disable-slicing", TRUE,
                          NULL);

  g_signal_connect (CLUTTER_TEXTURE (texture),
		    "size-change",
		    G_CALLBACK (size_change), NULL);

  /* Set up pipeline */
  pipeline = GST_PIPELINE(gst_pipeline_new (NULL));

  src = gst_element_factory_make ("videotestsrc", NULL);
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  colorspace = gst_element_factory_make ("ffmpegcolorspace", NULL);
  sink = clutter_gst_video_sink_new (CLUTTER_TEXTURE (texture));

  g_object_set (G_OBJECT (sink), "use-shaders", FALSE, NULL);

  /* make videotestsrc spit the format we want */
  caps = gst_caps_new_simple ("video/x-raw-rgb",
         "bpp", G_TYPE_INT, opt_bpp,
         "depth", G_TYPE_INT, opt_depth,
         "framerate", GST_TYPE_FRACTION, opt_framerate, 1,
#if 0
	 "red_mask", G_TYPE_INT, 0xff000000,
	 "green_mask", G_TYPE_INT, 0x00ff0000,
	 "blue_mask", G_TYPE_INT, 0x0000ff00,
#endif
	 NULL);
  g_object_set (capsfilter, "caps", caps, NULL);

  g_printf ("%s: [caps] %s\n", __FILE__, gst_caps_to_string (caps));
  gst_bin_add_many (GST_BIN (pipeline), src, capsfilter, sink, NULL);
  result = gst_element_link_many (src, capsfilter, sink, NULL);
  if (result == FALSE)
      g_critical("Could not link elements");
  gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_PLAYING);

  clutter_group_add (CLUTTER_GROUP (stage), texture);
  clutter_actor_show_all (stage);

  clutter_main();

  return 0;
}
