source_h = 					\
	$(srcdir)/clutter-gst-video-texture.h 	\
	$(srcdir)/clutter-gst-video-sink.h 	\
	$(srcdir)/clutter-gst-audio.h 		\
	$(srcdir)/clutter-gst-util.h		\
	$(srcdir)/clutter-gst-version.h		\
	$(srcdir)/clutter-gst-shaders.h


source_c = clutter-gst-video-texture.c \
	   clutter-gst-video-sink.c    \
	   clutter-gst-audio.c         \
           clutter-gst-util.c

libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_SOURCES = $(MARSHALFILES) \
                                                     $(source_c)     \
                                                     $(source_h)

INCLUDES = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"Clutter-Gst\" \
	@GCC_FLAGS@ \
	@CLUTTER_GST_CFLAGS@ \
	$(GST_CFLAGS)

lib_LTLIBRARIES = libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la

libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_LIBADD  = @CLUTTER_GST_LIBS@ $(GST_LIBS)
libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_LDFLAGS = @CLUTTER_GST_LT_LDFLAGS@

cluttergstheadersdir = $(includedir)/clutter-0.7/clutter-gst
cluttergstheaders_HEADERS = $(source_h) clutter-gst.h
