#!/bin/sh
export WANT_AUTOMAKE=1.6
aclocal
aclocal -I m4
autoheader
cp configure.ac configure.ac.save
cp Makefile.am Makefile.am.save
gettextize --copy --no-changelog
mv configure.ac.save configure.ac
mv Makefile.am.save Makefile.am
libtoolize --automake --copy
automake --add-missing --copy
autoconf
