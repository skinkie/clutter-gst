#include <clutter-gst/clutter-gst.h>

int
main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterActor     *stage, *label;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterGstAudio  *audio;

  if (argc < 2) {
          g_error ("Usage: %s URI", argv[0]);
          return 1;
  }
  
  clutter_init (&argc, &argv);
  gst_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a label */
  label = clutter_label_new ();
  clutter_label_set_text (CLUTTER_LABEL (label), "Music");
  clutter_actor_set_position (label, 100, 100);
  clutter_group_add (CLUTTER_GROUP (stage), label);

  /* Make a timeline */
  timeline = clutter_timeline_new (100, 30); /* num frames, fps */
  g_object_set(timeline, "loop", TRUE, NULL);

  /* Set up audio player */
  audio = clutter_gst_audio_new ();
  clutter_media_set_uri (CLUTTER_MEDIA (audio), argv[1]);
  clutter_media_set_playing (CLUTTER_MEDIA (audio), TRUE);

  /* start the timeline */
  clutter_timeline_start (timeline);

  clutter_group_show_all (CLUTTER_GROUP (stage));

  clutter_main();

  return 0;
}
