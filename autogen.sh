#!/bin/sh
export WANT_AUTOMAKE=1.6
aclocal
autoheader
gettextize --copy
libtoolize --automake --copy
automake --add-missing --copy
autoconf
