/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * audio-player.c - A simple audio player.
 *
 * Copyright (C) 2007,2008 OpenedHand
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

#include <clutter-gst/clutter-gst.h>

int
main (int argc, char *argv[])
{
  ClutterActor     *stage, *label;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterGstAudio  *audio;

  clutter_gst_init (&argc, &argv);

  if (argc < 2)
    {
      g_print ("Usage: [OPTIONS] %s URI\n", argv[0]);
      return EXIT_FAILURE;
    }

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

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
