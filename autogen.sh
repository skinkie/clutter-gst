#! /bin/sh
gtkdocize || exit 1
autoreconf -v --install || exit 1
if test -z "$NOCONFIGURE"; then
    ./configure "$@" || exit 1
fi
