NULL = #

SUBDIRS = shaders

source_h = 					\
	$(srcdir)/clutter-gst-audio.h 		\
	$(srcdir)/clutter-gst-util.h		\
	$(srcdir)/clutter-gst-version.h		\
	$(srcdir)/clutter-gst-video-sink.h 	\
	$(srcdir)/clutter-gst-video-texture.h 	\
	$(NULL)


source_priv_h =					\
	$(srcdir)/clutter-gst-debug.h		\
	$(srcdir)/clutter-gst-shaders.h		\
	$(NULL)

source_c = 					\
	   clutter-gst-audio.c			\
	   clutter-gst-debug.c			\
	   clutter-gst-video-sink.c		\
	   clutter-gst-video-texture.c		\
           clutter-gst-util.c			\
	   $(NULL)

libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_SOURCES = $(MARSHALFILES)  \
                                                     $(source_c)      \
						     $(source_priv_h) \
                                                     $(source_h)

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)/clutter-gst/shaders	\
	-I$(top_srcdir)/clutter-gst/shaders	\
	$(NULL)

AM_CFLAGS = 				\
	-DG_LOG_DOMAIN=\"Clutter-Gst\"	\
	$(MAINTAINER_CFLAGS)		\
	$(CLUTTER_GST_DEBUG_CFLAGS)	\
	$(CLUTTER_GST_CFLAGS)		\
	$(GST_CFLAGS)			\
	$(NULL)

lib_LTLIBRARIES = libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la

libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_LIBADD  = @CLUTTER_GST_LIBS@ $(GST_LIBS)
libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_LDFLAGS = @CLUTTER_GST_LT_LDFLAGS@

cluttergstheadersdir = $(includedir)/clutter-@CLUTTER_API_VERSION@/clutter-gst
cluttergstheaders_HEADERS = $(source_h) clutter-gst.h
