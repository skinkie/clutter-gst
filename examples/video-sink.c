#include <clutter-gst/clutter-gst.h>

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
  ClutterTimeline  *timeline;
  ClutterActor     *stage;
  ClutterActor     *texture;
  GstPipeline      *pipeline;
  GstElement       *src;
  GstElement       *warp;
  GstElement       *colorspace;
  GstElement       *sink;

  if (argc < 1) {
          g_error ("Usage: %s", argv[0]);
          return 1;
  }

  clutter_init (&argc, &argv);
  gst_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  /* Make a timeline */
  timeline = clutter_timeline_new (100, 30); /* num frames, fps */
  g_object_set(timeline, "loop", TRUE, 0);

  /* We need to set certain props on the target texture currently for
   * efficient/corrent playback onto the texture (which sucks a bit)  
  */
  texture = g_object_new (CLUTTER_TYPE_TEXTURE, 
			  "sync-size",    FALSE, 
			  "tiled",        FALSE, 
			  NULL);

  g_signal_connect (CLUTTER_TEXTURE (texture),
		    "size-change",
		    G_CALLBACK (size_change), NULL);

  /* Set up pipeline */
  pipeline = GST_PIPELINE(gst_pipeline_new (NULL));

  src = gst_element_factory_make ("videotestsrc", NULL);
  warp = gst_element_factory_make ("warptv", NULL);
  colorspace = gst_element_factory_make ("ffmpegcolorspace", NULL);
  sink = clutter_gst_video_sink_new (CLUTTER_TEXTURE (texture));

  // g_object_set (src , "pattern", 10, NULL);

  gst_bus_add_signal_watch (GST_ELEMENT_BUS (pipeline));

  gst_bin_add_many (GST_BIN (pipeline), src, warp, colorspace, sink, NULL);
  gst_element_link_many (src, warp, colorspace, sink, NULL);
  gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_PLAYING);

  /* start the timeline */
  clutter_timeline_start (timeline);

  clutter_group_add (CLUTTER_GROUP (stage), texture);
  // clutter_actor_set_opacity (texture, 0x11);
  clutter_actor_show_all (CLUTTER_GROUP (stage));

  clutter_main();

  return 0;
}
