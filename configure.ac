# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress developmen
# An even micro number indicates a released version.
m4_define(clutter_gst_major_version, 1)
m4_define(clutter_gst_minor_version, 5)
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

AM_INIT_AUTOMAKE([1.9 -Wno-portability foreign no-dist-gzip dist-xz tar-ustar])

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

AC_CANONICAL_HOST
AC_MSG_CHECKING([if building for some Win32 platform])
AS_CASE([$host],
        [*-*-mingw*|*-*-cygwin*],
        [
          CLUTTER_GST_LT_LDFLAGS="$CLUTTER_GST_LT_LDFLAGS -no-undefined"
          platform_win32=yes
        ],

        [platform_win32=no]
)
AC_MSG_RESULT([$platform_win32])

AC_SUBST(CLUTTER_GST_LT_VERSION)
AC_SUBST(CLUTTER_GST_LT_LDFLAGS)

m4_define([clutter_gst_release_status],
          [m4_if(m4_eval(clutter_gst_micro_version % 2), [1], [git],
                 [m4_if(m4_eval(clutter_gst_minor_version % 2), [1], [snapshot],
                       [release])])])
AC_SUBST([CLUTTER_GST_RELEASE_STATUS], [clutter_gst_release_status])

# pkg-config requirements
GLIB_REQ_VERSION=2.18.0
CLUTTER_REQ_VERSION=1.6.0
GSTREAMER_REQ_VERSION=0.10.26

AC_SUBST(GLIB_REQ_VERSION)
AC_SUBST(CLUTTER_REQ_VERSION)
AC_SUBST(GSTREAMER_REQ_VERSION)

dnl ========================================================================

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AS_AM_REALLY_SILENT

dnl ========================================================================

# require libtool >= 2.2
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_SED
DOLT
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

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

pkg_modules="clutter-1.0 >= $CLUTTER_REQ_VERSION gio-2.0 >= $GLIB_REQ_VERSION"
PKG_CHECK_MODULES(CLUTTER_GST, [$pkg_modules])

dnl ========================================================================

GST_MAJORMINOR=0.10

pkg_modules="gstreamer-$GST_MAJORMINOR >= $GSTREAMER_REQ_VERSION gstreamer-plugins-base-$GST_MAJORMINOR gstreamer-base-$GST_MAJORMINOR gstreamer-interfaces-$GST_MAJORMINOR gstreamer-video-$GST_MAJORMINOR gstreamer-audio-$GST_MAJORMINOR gstreamer-tag-$GST_MAJORMINOR"
PKG_CHECK_MODULES(GST, [$pkg_modules])

dnl ========================================================================
dnl GL inclusion and link. We depend on GL for the single glGetIntegerv()
dnl call. The end goal is to get rid of it, but we can't just yet.

dnl For now, we assume Clutter/Cogl includes the GL headers (whatever they
dnl are). By the time they don't do that, we'll hopefully won't have any
dnl direct GL dependency.

dnl As for linking against -lGL, I believe Clutter always end up puttin libGL
dnl in its exported CFLAGS, except when compiling against the NVidia library
dnl (which does not have the 'gl' .pc file. For now, let's just add -lGL when
dnl we are using the glx flavour, That should fix the vast majority of the
dnl issues

dnl Why don't we "just" link against GL? check the cogl/clutter configure.ac :)
dnl Hopefully this will be enough for the time being.

dnl With Cogl 1.8.0 all of the above is moot because we can use
dnl cogl_renderer_get_n_fragment_texture_units() and never depend on GL/GLES
dnl directly. However let's not hard depend on cogl 1.8.0 just yet.
dnl Well, obviously just a little detail remaining, it's not possible to grab
dnl the CoglContext on OS X yet.

PKG_CHECK_EXISTS([clutter-osx-1.0],
                 [
                   AC_DEFINE([HAVE_CLUTTER_OSX], [1],
                             ["Defined if building Clutter for OS X"])
                 ])
PKG_CHECK_EXISTS([cogl-1.0 >= 1.8.0],
                 [
                   have_cogl_1_8=yes
                   AC_DEFINE([HAVE_COGL_1_8], [1],
                             ["Defined if cogl-1.0 >= 1.8.0 is available"])
                 ])

AS_IF([test "x$have_cogl_1_8" != xyes],
      [
        clutter_soname=`$PKG_CONFIG --variable soname_infix clutter-1.0`
        AS_CASE([$clutter_soname],
                [glx],
                [AC_CHECK_LIB([GL], [glGetIntegerv], [GL_LDFLAGS="-lGL"])])
        AC_SUBST([GL_LDFLAGS])
      ])

dnl ========================================================================

m4_define([maintainer_default], [m4_if(clutter_gst_release_status,
                                       [git], [yes], [no])])
AC_ARG_ENABLE([maintainer-flags],
              [AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                              [Use strict compiler flags @<:@default=]maintainer_default[@:>@])],
              [],
              enable_maintainer_flags=maintainer_default)

AS_IF([test "x$enable_maintainer_flags" = "xyes" && test "x$GCC" = "xyes"],
      [
        AS_COMPILER_FLAGS([MAINTAINER_CFLAGS],
                          ["-Wall -Wshadow -Wcast-align -Wno-uninitialized
                            -Wno-strict-aliasing -Wempty-body -Wformat-security
                            -Winit-self -Wsign-compare"])
      ]
)

AC_SUBST(MAINTAINER_CFLAGS)

dnl === Enable debug level ====================================================

m4_define([debug_default], [m4_if(clutter_gst_release_status,
                                  [git], [yes], [minimum])])
AC_ARG_ENABLE([debug],
              [AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                              [Control Clutter-Gst debugging level @<:@default=]debug_default[@:>@])],
              [],
              enable_debug=debug_default)

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

AC_OUTPUT([
        Makefile
        build/Makefile
        build/autotools/Makefile
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
echo "Clutter-GST $VERSION (${CLUTTER_GST_RELEASE_STATUS})"
echo ""
echo " • Global:"
echo "        Prefix     : ${prefix}"
echo "        Cg compiler: ${have_cgc}"
echo ""
echo " • Compiler options:"
echo "        Compiler flags:${MAINTAINER_CFLAGS} ${CPPFLAGS}"
echo "        Debug level   : ${enable_debug}"
echo ""
echo " • Extra:"
echo "        API documentation : ${enable_gtk_doc}"
echo "        Introspection data: ${enable_introspection}"
echo ""
