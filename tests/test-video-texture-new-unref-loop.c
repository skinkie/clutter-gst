/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * test-video-texture-new-unref-loop.c - Create and free a Video texture in a
 * tight loop to quickly check if we are leaking or not.
 *
 * Copyright (C) 2011 Intel Corporation
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
 *
 * Authors:
 *   Damien Lespiau  <damien.lespiau@intel.com>
 */

#include <stdlib.h>

#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

int
main (int argc, char *argv[])
{
  ClutterActor *vtexture;
  int i;

  clutter_gst_init (&argc, &argv);

  for (i = 0; ; i++)
  {
    g_debug("VideoTexure #%d", i);
    vtexture = clutter_gst_video_texture_new();
    g_object_ref_sink (vtexture);
    g_object_unref (vtexture);
  }

  return EXIT_SUCCESS;
}
