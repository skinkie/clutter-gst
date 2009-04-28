NULL =

noinst_PROGRAMS = test-yuv-upload \
                  test-rgb-upload \
		  $(NULL)

INCLUDES = -I$(top_srcdir)/

test_yuv_upload_SOURCES = test-yuv-upload.c
test_yuv_upload_CFLAGS  = $(CLUTTER_GST_CFLAGS) $(GST_CFLAGS)
test_yuv_upload_LDFLAGS = \
    $(CLUTTER_GST_LIBS)   \
    $(GST_LIBS)           \
    $(top_builddir)/clutter-gst/libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la

test_rgb_upload_SOURCES = test-rgb-upload.c
test_rgb_upload_CFLAGS  = $(CLUTTER_GST_CFLAGS) $(GST_CFLAGS)
test_rgb_upload_LDFLAGS = \
    $(CLUTTER_GST_LIBS)   \
    $(GST_LIBS)           \
    $(top_builddir)/clutter-gst/libclutter-gst-@CLUTTER_GST_MAJORMINOR@.la
