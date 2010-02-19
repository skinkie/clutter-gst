include $(top_srcdir)/build/autotools/Makefile.am.silent

NULL = #

CLEANFILES =
EXTRA_DIST = I420.cg I420.pso I420.h \
             YV12.cg YV12.pso YV12.h \
             $(NULL)

#
# If we have python we are able to build the headers holding the fragment
# programs.
#

if HAVE_PYTHON

BUILT_SOURCES = I420.h \
                YV12.h \
                $(NULL)

CLEANFILES += I420.h \
              YV12.h \
              $(NULL)

# Don't use GNU make extension to avoid automake warnings, yeah I know, that's
# annoying.

I420.h: I420.pso
	$(QUIET_GEN)$(top_srcdir)/scripts/pso2h.py -n I420_fp -o $@ $^

YV12.h: YV12.pso
	$(QUIET_GEN)$(top_srcdir)/scripts/pso2h.py -n YV12_fp -o $@ $^

endif

#
# Only add those targets if we the Cg compiler is around.
#

if HAVE_CGC

CLEANFILES += I420.pso \
              YV12.pso \
              $(NULL)

I420.pso: I420.cg
	$(QUIET_CGC)cgc -entry I420_main -profile arbfp1 -o $@ $^

YV12.pso: YV12.cg
	$(QUIET_CGC)cgc -entry YV12_main -profile arbfp1 -o $@ $^

endif
