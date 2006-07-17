#!/bin/sh
export WANT_AUTOMAKE=1.6
aclocal
autoheader
cp configure.ac configure.ac.save
cp Makefile.am Makefile.am.save
gettextize --copy --no-changelog
aclocal -I m4
mv configure.ac.save configure.ac
mv Makefile.am.save Makefile.am
libtoolize --automake --copy
automake --add-missing --copy
autoconf
