/*
 * Clutter-GStreamer.
 *
 * GStreamer integration library for Clutter.
 *
 * clutter-gst-debug.h - Debugging (printf) functions
 *
 * Copyright (C) 2006-2008 OpenedHand
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

#ifndef __CLUTTER_GST_DEBUG_H__
#define __CLUTTER_GST_DEBUG_H__

#include <glib.h>

G_BEGIN_DECLS

#ifdef CLUTTER_GST_ENABLE_DEBUG

typedef enum {
  CLUTTER_GST_DEBUG_MISC            = 1 << 0,
  CLUTTER_GST_DEBUG_MEDIA           = 1 << 1,
  CLUTTER_GST_DEBUG_ASPECT_RATIO    = 1 << 2
} ClutterDebugFlag;

#ifdef __GNUC__

#define CLUTTER_GST_NOTE(type,x,a...)                         \
  G_STMT_START {                                              \
      if (clutter_gst_debug_flags & CLUTTER_GST_DEBUG_##type) \
        { g_message ("[" #type "] " G_STRLOC ": " x, ##a); }  \
  } G_STMT_END

#define CLUTTER_GST_TIMESTAMP(type,x,a...)                    \
  G_STMT_START {                                              \
      if (clutter_gst_debug_flags & CLUTTER_GST_DEBUG_##type) \
        { g_message ("[" #type "]" " %li:"  G_STRLOC ": "     \
                     x, _clutter_gst_get_timestamp(), ##a); } \
  } G_STMT_END

#else /* !__GNUC__ */

/* Try the C99 version; unfortunately, this does not allow us to pass
 * empty arguments to the macro, which means we have to
 * do an intemediate printf.
 */
#define CLUTTER_GST_NOTE(type,...)                            \
  G_STMT_START {                                              \
      if (clutter_gst_debug_flags & CLUTTER_GST_DEBUG_##type) \
        {                                                     \
          gchar * _fmt = g_strdup_printf (__VA_ARGS__);       \
          g_message ("[" #type "] " G_STRLOC ": %s",_fmt);    \
          g_free (_fmt);                                      \
        }                                                     \
  } G_STMT_END

#define CLUTTER_GST_TIMESTAMP(type,...)                       \
G_STMT_START {                                                \
    if (clutter_gst_debug_flags & CLUTTER_GST_DEBUG_##type)   \
      {                                                       \
        gchar * _fmt = g_strdup_printf (__VA_ARGS__);         \
        g_message ("[" #type "]" " %li:"  G_STRLOC ": %s",    \
                   _clutter_gst_get_timestamp(), _fmt);       \
        g_free (_fmt);                                        \
      }                                                       \
} G_STMT_END

#endif /* __GNUC__ */

#define CLUTTER_GST_MARK()      CLUTTER_GST_NOTE(MISC, "== mark ==")

#define CLUTTER_GST_GLERR()                                   \
G_STMT_START {                                                \
    if (clutter_gst_debug_flags & CLUTTER_GST_DEBUG_GL)       \
      { GLenum _err = glGetError (); /* roundtrip */          \
        if (_err != GL_NO_ERROR)                              \
          g_warning (G_STRLOC ": GL Error %x", _err);         \
      }                                                       \
} G_STMT_END

/* We do not even define those (private) symbols when debug is disabled.
 * This is to ensure the debug code is not shiped with the library when
 * disabled */

extern guint clutter_gst_debug_flags;

gulong    _clutter_gst_get_timestamp    (void);
gboolean  _clutter_gst_debug_init       (void);

#else /* !CLUTTER_GST_ENABLE_DEBUG */

#define CLUTTER_GST_NOTE(type,...)         G_STMT_START { } G_STMT_END
#define CLUTTER_GST_MARK()                 G_STMT_START { } G_STMT_END
#define CLUTTER_GST_GLERR()                G_STMT_START { } G_STMT_END
#define CLUTTER_GST_TIMESTAMP(type,...)    G_STMT_START { } G_STMT_END

#endif /* CLUTTER_GST_ENABLE_DEBUG */

G_END_DECLS

#endif /* __CLUTTER_GST_DEBUG_H__ */
