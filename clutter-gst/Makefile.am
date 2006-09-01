source_h =				      \
	$(srcdir)/clutter-gst-video-texture.h \
	$(srcdir)/clutter-gst-audio.h

source_c = clutter-gst-video-texture.c \
	   clutter-gst-audio.c

libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_SOURCES = $(MARSHALFILES) \
                                                     $(source_c)     \
                                                     $(source_h)

INCLUDES = @GCC_FLAGS@ @CLUTTER_GST_CFLAGS@  $(GST_CFLAGS) -I$(top_srcdir)

lib_LTLIBRARIES = libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la

libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_LIBADD  = @CLUTTER_GST_LIBS@ $(GST_LIBS)
libclutter_gst_@CLUTTER_GST_MAJORMINOR@_la_LDFLAGS = @CLUTTER_GST_LT_LDFLAGS@

cluttergstheadersdir = $(includedir)/clutter-gst-@CLUTTER_GST_MAJORMINOR@/clutter-gst
cluttergstheaders_HEADERS = $(source_h) 		\
		            clutter-gst.h
