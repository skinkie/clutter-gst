# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress developmen
# An even micro number indicates a released version.
m4_define(clutter_gst_major_version, 1)
m4_define(clutter_gst_minor_version, 0)
m4_define(clutter_gst_micro_version, 1)

m4_define(clutter_api_version, 1.0)
m4_define([clutter_gst_api_version], [1.0])

m4_define([clutter_gst_version],
          [clutter_gst_major_version.clutter_gst_minor_version.clutter_gst_micro_version])

# increase the interface age for each release; if the API changes, set to 0
m4_define([clutter_gst_interface_age], [0])
m4_define([clutter_gst_binary_age],
          [m4_eval(100 * clutter_gst_minor_version + clutter_gst_micro_version)])

AC_PREREQ(2.53)

AC_INIT([clutter-gst], [clutter_gst_version],
        [http://bugzilla.o-hand.com/enter_bug.cgi?product=Clutter-gst])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_AUX_DIR([build/autotools])
AC_CONFIG_SRCDIR([clutter-gst/clutter-gst.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.9 -Wno-portability])

CLUTTER_GST_MAJOR_VERSION=clutter_gst_major_version
CLUTTER_GST_MINOR_VERSION=clutter_gst_minor_version
CLUTTER_GST_MICRO_VERSION=clutter_gst_micro_version
CLUTTER_GST_VERSION=clutter_gst_version
CLUTTER_GST_API_VERSION=clutter_gst_api_version
CLUTTER_GST_MAJORMINOR=clutter_gst_api_version
CLUTTER_API_VERSION=clutter_api_version
AC_SUBST(CLUTTER_GST_MAJOR_VERSION)
AC_SUBST(CLUTTER_GST_MINOR_VERSION)
AC_SUBST(CLUTTER_GST_MICRO_VERSION)
AC_SUBST(CLUTTER_GST_VERSION)
AC_SUBST(CLUTTER_GST_API_VERSION)
AC_SUBST(CLUTTER_GST_MAJORMINOR)
AC_SUBST(CLUTTER_API_VERSION)

m4_define([lt_current],
          [m4_eval(100 * clutter_gst_minor_version + clutter_gst_micro_version - clutter_gst_interface_age)])
m4_define([lt_revision], [clutter_gst_interface_age])
m4_define([lt_age], [m4_eval(clutter_gst_binary_age - clutter_gst_interface_age)])

CLUTTER_GST_LT_CURRENT=lt_current
CLUTTER_GST_LT_REV=lt_revision
CLUTTER_GST_LT_AGE=lt_age
CLUTTER_GST_LT_VERSION="$CLUTTER_GST_LT_CURRENT:$CLUTTER_GST_LT_REV:$CLUTTER_GST_LT_AGE"
CLUTTER_GST_LT_LDFLAGS="-version-info $CLUTTER_GST_LT_VERSION"

AC_SUBST(CLUTTER_GST_LT_VERSION)
AC_SUBST(CLUTTER_GST_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
DOLT

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset munmap strcasecmp strdup])

dnl ========================================================================

AC_CHECK_PROG(CGC, [cgc], [cgc], [no])
have_cgc="no"
if test "x$CGC" = "xcgc"; then
        have_cgc="yes"
fi
AM_CONDITIONAL([HAVE_CGC], [test "x$have_cgc" = "xyes"])

dnl ========================================================================
dnl check for python (pso2h.py needs it)

AC_CHECK_PROG(PYTHON, [python], [python], [no])
have_python="no"
if test "x$PYTHON" = "xpython"; then
        have_python="yes"
fi
AM_CONDITIONAL([HAVE_PYTHON], [test "x$have_python" = "xyes"])

dnl ========================================================================

pkg_modules="clutter-1.0 >= 1.0.0 gio-2.0"
PKG_CHECK_MODULES(CLUTTER_GST, [$pkg_modules])

dnl ========================================================================

GST_MAJORMINOR=0.10

pkg_modules="gstreamer-$GST_MAJORMINOR gstreamer-plugins-base-$GST_MAJORMINOR gstreamer-base-$GST_MAJORMINOR"
PKG_CHECK_MODULES(GST, [$pkg_modules])

GST_LIBS="$GST_LIBS -lgstinterfaces-$GST_MAJORMINOR -lgstvideo-$GST_MAJORMINOR -lgstaudio-$GST_MAJORMINOR"

dnl ========================================================================

m4_define([maintainer_flags_default], [yes])
AC_ARG_ENABLE([maintainer-flags],
              [AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                              [Use strict compiler flags @<:@default=maintainer_flags_default@:>@])],
              [],
              [enable_maintainer_flags=maintainer_flags_default])

AS_IF([test "x$enable_maintainer_flags" = "xyes" && test "x$GCC" = "xyes"],
      [
        AS_COMPILER_FLAGS([MAINTAINER_CFLAGS],
                          ["-Wall -Wshadow -Wcast-align -Wno-uninitialized
                            -Wno-strict-aliasing -Wempty-body -Wformat-security
                            -Winit-self"])
      ]
)

AC_SUBST(MAINTAINER_CFLAGS)

dnl === Enable debug level ====================================================

m4_define([debug_default], [m4_if(m4_eval(clutter_gst_minor_version % 2), [1], [yes], [minimum])])
AC_ARG_ENABLE([debug],
              [AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                              [Control Clutter-Gst debugging level @<:@default=debug_default@:>@])],
              [],
              [enable_debug=debug_default])

AS_CASE([$enable_debug],

        [yes],
        [
          test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
          CLUTTER_GST_DEBUG_CFLAGS="-DCLUTTER_GST_ENABLE_DEBUG"
        ],

        [minimum],
        [CLUTTER_GST_DEBUG_CFLAGS="-DCLUTTER_GST_ENABLE_DEBUG -DG_DISABLE_CAST_CHECKS"],

        [no],
        [CLUTTER_GST_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"],

        [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)

AC_SUBST(CLUTTER_GST_DEBUG_CFLAGS)

dnl ========================================================================

GTK_DOC_CHECK([1.8])

AM_CONDITIONAL([BUILD_GTK_DOC],
               [test "x$enable_gtk_doc" = "xyes" || test ! -f "autogen.sh"])

dnl ========================================================================

GOBJECT_INTROSPECTION_CHECK([0.6.8])

dnl ========================================================================

AC_SUBST(GCC_FLAGS)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

AC_SUBST(CLUTTER_GST_CFLAGS)
AC_SUBST(CLUTTER_GST_LIBS)

SHAVE_INIT([build/autotools], [enable])

AC_OUTPUT([
        Makefile
        build/Makefile
        build/autotools/Makefile
        build/autotools/shave
        build/autotools/shave-libtool
        scripts/Makefile
        clutter-gst/Makefile
        clutter-gst/clutter-gst-version.h
        clutter-gst/shaders/Makefile
        tests/Makefile
        examples/Makefile
        doc/Makefile
        doc/reference/Makefile
        doc/reference/version.xml
        clutter-gst.pc
])

dnl ========================================================================

echo ""
echo "                  Clutter-GST $VERSION"
echo "                 ==================="
echo ""
echo "                   prefix:   ${prefix}"
echo "           Compiler flags:   ${MAINTAINER_CFLAGS} ${CPPFLAGS}"
echo ""
echo "              Cg compiler:   ${have_cgc}"
echo ""
echo "              debug level:   ${enable_debug}"
echo "  Build API documentation:   ${enable_gtk_doc}"
echo "       Introspection data:   ${enable_introspection}"
echo ""
