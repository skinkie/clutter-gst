/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-util.h - Miscellaneous functions
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#include "clutter-gst-util.h"
#include <gst/gst.h>

/**
 * SECTION:clutter-gst-util
 * @short_description: Utility functions for ClutterGst.
 *
 * Various Utility functions for ClutterGst.
 */

/**
 * clutter_gst_init:
 *
 * Utility function to call gst_init, then clutter_init. 
 *
 * Return value: A #ClutterInitError.
 */
ClutterInitError
clutter_gst_init (int    *argc,
		  char ***argv)
{
  static gboolean gst_is_initialized = FALSE;

  if (!gst_is_initialized)
    gst_init (argc, argv);

  gst_is_initialized = TRUE;

  return clutter_init (argc, argv);
}
