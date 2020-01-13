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

AC_CHECK_FUNC([X509_check_host],
	[AC_DEFINE([HAVE_X509_CHECK_HOST], [1], [define if X509_check_host exists])],
	dnl	[AC_ERROR([X509_check_host not found])]
)

# For taking in config.h
CPPFLAGS="-I\$(abs_top_srcdir)/src ${CPPFLAGS}"

dnl For building against GENF.
dnl CPPFLAGS="-I\$(abs_top_srcdir)/genf/include ${CPPFLAGS}"
dnl LDFLAGS="-L\$(abs_top_builddir)/genf/.libs ${LDFLAGS}"
GENF="\$(abs_top_builddir)/src/genf"

AC_CHECK_KERNDIR

dnl For building against kring.
KRING_MOD="\$(abs_top_builddir)/kring/kring.ko"
KRING_SYMVERS="\$(abs_top_builddir)/kring/Module.symvers"
CPPFLAGS="-I\$(abs_top_builddir)/kring/include ${CPPFLAGS}"
LDFLAGS="-L\$(abs_top_builddir)/kring ${LDFLAGS}"
SED_SUBST="$SED_SUBST -e 's|[@]KRING_MOD[@]|$KRING_MOD|g'"

AC_SUBST(GENF)
AC_SUBST(KRING_SYMVERS)
AC_SUBST(KRING_MOD)
AC_SUBST(SED_SUBST)

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	kring/Makefile
	shuttle/Makefile
])

AC_OUTPUT
