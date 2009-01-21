#include <clutter-gst/clutter-gst.h>

int
main (int argc, char *argv[])
{
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
  label = clutter_text_new ();
  clutter_text_set_text (CLUTTER_TEXT (label), "Music");
  clutter_actor_set_position (label, 100, 100);
  clutter_group_add (CLUTTER_GROUP (stage), label);

  /* Set up audio player */
  audio = clutter_gst_audio_new ();
  clutter_media_set_uri (CLUTTER_MEDIA (audio), argv[1]);
  clutter_media_set_playing (CLUTTER_MEDIA (audio), TRUE);
  clutter_media_set_audio_volume (CLUTTER_MEDIA (audio), 0.5);

  clutter_actor_show_all (stage);

  clutter_main();

  return 0;
}
