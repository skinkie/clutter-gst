/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-debug.c - Some debug related functions, private to the library.
 *
 * Authored By Damien Lespiau <damien.lespiau@intel.com>
 *
 * Copyright (C) 2009 Intel Corporation
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

#include <gst/gst.h>

#include "clutter-gst-debug.h"

#ifdef CLUTTER_GST_ENABLE_DEBUG

guint clutter_gst_debug_flags = 0;  /* global clutter-gst debug flag */

static GTimer *clutter_gst_timer;

static const GDebugKey clutter_gst_debug_keys[] = {
  { "misc",         CLUTTER_GST_DEBUG_MISC },
  { "media",        CLUTTER_GST_DEBUG_MEDIA },
  { "aspect-ratio", CLUTTER_GST_DEBUG_ASPECT_RATIO }
};

/**
 * clutter_gst_get_timestamp:
 *
 * Returns the approximate number of microseconds passed since Clutter-Gst was
 * intialized.
 *
 * Return value: Number of microseconds since clutter_gst_init() was called.
 */
gulong
_clutter_gst_get_timestamp (void)
{
  gdouble seconds;

  seconds = g_timer_elapsed (clutter_gst_timer, NULL);

  return (gulong)(seconds / 1.0e-6);
}

gboolean _clutter_gst_debug_init (void)
{
  const char *env_string;

  env_string = g_getenv ("CLUTTER_GST_DEBUG");

  clutter_gst_timer = g_timer_new ();
  g_timer_start (clutter_gst_timer);

  if (env_string == NULL)
    return TRUE;

  clutter_gst_debug_flags =
    g_parse_debug_string (env_string,
                          clutter_gst_debug_keys,
                          G_N_ELEMENTS (clutter_gst_debug_keys));

  return TRUE;
}

#endif /* CLUTTER_GST_ENABLE_DEBUG */

