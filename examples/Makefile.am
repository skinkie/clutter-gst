noinst_PROGRAMS = video-player video-cube audio

INCLUDES = -I$(top_srcdir)/

video_player_SOURCES = video-player.c
video_player_CFLAGS = $(CLUTTER_GST_CFLAGS) $(GST_CFLAGS)
video_player_LDFLAGS = \
    $(CLUTTER_GST_LIBS) \
    $(GST_LIBS)   \
    $(top_builddir)/clutter-gst/libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la 

video_cube_SOURCES = video-cube.c
video_cube_CFLAGS = $(CLUTTER_GST_CFLAGS) $(GST_CFLAGS)
video_cube_LDFLAGS = \
    $(CLUTTER_LIBS) \
    $(GST_LIBS)   \
    $(top_builddir)/clutter-gst/libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la 

audio_SOURCES = audio.c
audio_CFLAGS = $(CLUTTER_GST_CFLAGS) $(GST_CFLAGS)
audio_LDFLAGS = \
    $(CLUTTER_GST_LIBS) \
    $(GST_LIBS)   \
    $(top_builddir)/clutter-gst/libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la 

EXTRA_DIST = media-actions-pause.png  \
             media-actions-start.png  \
             vid-panel.png            \
             README 
