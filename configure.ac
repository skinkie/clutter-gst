AC_PREREQ(2.53)

# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development, (eg. from CVS)
# An even micro number indicates a released version.
m4_define(clutter_gst_version_major, 0)
m4_define(clutter_gst_version_minor, 3)
m4_define(clutter_gst_version_micro, 0)

AC_INIT([clutter-gst], 
       clutter_gst_version_major.clutter_gst_version_minor.clutter_gst_version_micro, 
        [http://bugzilla.o-hand.com/enter_bug.cgi?product=Clutter-gst])
AC_CONFIG_SRCDIR([clutter-gst/clutter-gst.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.7])

CLUTTER_GST_MAJORMINOR=clutter_gst_version_major.clutter_gst_version_minor
AC_SUBST(CLUTTER_GST_MAJORMINOR)

# CURRENT, REVISION, AGE
# - library source changed -> increment REVISION
# - interfaces added/removed/changed -> increment CURRENT, REVISION = 0
# - interfaces added -> increment AGE
# - interfaces removed -> AGE = 0
CLUTTER_GST_LT_CURRENT=1
CLUTTER_GST_LT_REV=0
CLUTTER_GST_LT_AGE=1
CLUTTER_GST_LT_VERSION="$CLUTTER_GST_LT_CURRENT:$CLUTTER_GST_LT_REV:$CLUTTER_GST_LT_AGE"
CLUTTER_GST_LT_LDFLAGS="-version-info $CLUTTER_GST_LT_VERSION"

AC_SUBST(CLUTTER_GST_LT_VERSION)
AC_SUBST(CLUTTER_GST_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

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

pkg_modules="clutter-0.3"
PKG_CHECK_MODULES(CLUTTER_GST, [$pkg_modules])

dnl ========================================================================

GST_MAJORMINOR=0.10

pkg_modules="gstreamer-$GST_MAJORMINOR gstreamer-plugins-base-$GST_MAJORMINOR"
PKG_CHECK_MODULES(GST, [$pkg_modules])

GST_LIBS="$GST_LIBS -lgstinterfaces-$GST_MAJORMINOR -lgstvideo-$GST_MAJORMINOR -lgstaudio-$GST_MAJORMINOR"

dnl ========================================================================

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

dnl ========================================================================

GTK_DOC_CHECK([1.0])

dnl ========================================================================

AC_SUBST(GCC_FLAGS)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

AC_SUBST(CLUTTER_GST_CFLAGS)
AC_SUBST(CLUTTER_GST_LIBS)

AC_OUTPUT([Makefile
clutter-gst/Makefile
examples/Makefile
doc/Makefile
doc/reference/Makefile
doc/reference/version.xml
clutter-gst.pc
])

dnl ========================================================================

echo ""
echo "                 Clutter-GST $VERSION"
echo "                 ==================="
echo ""
echo "                   prefix:   ${prefix}"
echo ""
echo "            Documentation:   ${enable_gtk_doc}"
echo ""
