AC_INIT([servdef], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

m4_include([m4/common.m4])

AC_PROG_CC
AC_PROG_CXX

AC_CHECK_COLM([0.13.0])
AC_SUBST(SED_SUBST)

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(src/Makefile)

AC_OUTPUT
