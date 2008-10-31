SUBDIRS = clutter-gst examples

if BUILD_GTK_DOC
SUBDIRS += doc
endif

DIST_SUBDIRS = clutter-gst examples doc

clutter-gst-0.9.pc: clutter-gst.pc
	@cp -f clutter-gst.pc clutter-gst-0.9.pc

pkgconfig_DATA = clutter-gst-0.9.pc
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = clutter-gst.pc.in

CLEANFILES = clutter-gst-0.9.pc

DISTCLEANFILES = clutter-gst.pc

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub \
                       configure depcomp install-sh ltmain.sh     \
                       Makefile.in missing config.h.in
