#!/bin/sh
#

# Default location of build package
prefix=/opt/colm

# Can supply it on command line.
[ -n "$1" ] && prefix="$1"

$prefix/bin/autogen
