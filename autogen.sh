#!/bin/bash
#

message()
{
	echo "+ $1" >&2
}

message "copying macros to m4/"
[ -d m4 ] || mkdir m4
cp src/*.m4 m4/

message "copying sedsubst file to ./"
cp src/sedsubst ./
chmod +x ./sedsubst

set -x

libtoolize --copy --no-warn;
autoheader
aclocal
automake --foreign --include-deps --add-missing --copy
autoconf
