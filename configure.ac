AC_INIT([netp], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/common.m4])

: ${CFLAGS="-Wall -g"}
: ${CXXFLAGS="-Wall -g"}

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_LANG_CPLUSPLUS

dnl Colm
AC_CHECK_COLM([0.14])

dnl Usage dependencies are stored in pkgbuild/common.m4
AC_CHECK_GENF_DEPS()

AC_SUBST(SED_SUBST)

AC_CHECK_FUNC([X509_check_host],
	[AC_DEFINE([HAVE_X509_CHECK_HOST], [1], [define if X509_check_host exists])],
	dnl	[AC_ERROR([X509_check_host not found])]
)

# For taking in config.h
CPPFLAGS="-I\$(abs_top_srcdir)/src ${CPPFLAGS}"

AC_CHECK_KERNDIR

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	kring/Makefile
])

AC_OUTPUT
