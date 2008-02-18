SUBDIRS = clutter-gst doc examples 

clutter-gst-0.6.pc: clutter-gst.pc
	@cp -f clutter-gst.pc clutter-gst-0.6.pc

pkgconfig_DATA = clutter-gst-0.6.pc
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = clutter-gst.pc.in

CLEANFILES = clutter-gst-0.6.pc

DISTCLEANFILES = clutter-gst.pc

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub \
                       configure depcomp install-sh ltmain.sh     \
                       Makefile.in missing config.h.in
