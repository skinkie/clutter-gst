QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)
QUIET_CGC = $(Q:@=@echo '  CGC   '$@;)
NULL =

BUILT_SOURCES = I420.h

EXTRA_DIST = I420.cg I420.pso I420.h

CLEANFILES = I420.h

I420.h: I420.pso
	$(QUIET_GEN)$(top_srcdir)/scripts/pso2h.py -n I420_fp -o $@ $^

#
# Only add those targets if we the Cg compiler is around.
#

if HAVE_CGC

CLEANFILES += I420.pso

I420.pso: I420.cg
	$(QUIET_CGC)cgc -entry I420_main -profile arbfp1 -o $@ $^

endif
