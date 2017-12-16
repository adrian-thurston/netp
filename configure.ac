AC_INIT([shuttle], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/common.m4])

: ${CFLAGS="-Wall -g"}
: ${CXXFLAGS="-Wall -g"}

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_LANG_CPLUSPLUS

AC_CHECK_GENF([0.1.0])

AC_CHECK_KRING()

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

KERNDIR=`function f { echo $1; }; f /lib/modules/3.13.0-*/build`
AC_CHECK_FILE( [$KERNDIR/Module.symvers], [],
	[AC_ERROR([could not locate kernel build dir at $KERNDIR] )] )
AC_SUBST([KERNDIR])

AC_OUTPUT
