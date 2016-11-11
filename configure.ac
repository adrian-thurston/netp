AC_INIT([genf], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/common.m4])

: ${CFLAGS="-Wall -g"}
: ${CXXFLAGS="-Wall -g"}

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_LANG_CPLUSPLUS

dnl pthread
AC_CHECK_HEADER( [pthread.h] )
AC_CHECK_LIB( [pthread], [pthread_create] )

dnl openssl
AC_CHECK_HEADER( [openssl/openssl.h] )
AC_CHECK_LIB( [ssl], [SSL_library_init] )
AC_CHECK_LIB( [crypto], [CRYPTO_thread_id]  )

dnl c-ares (dns resolving)
AC_CHECK_HEADER( [ares.h] )
AC_CHECK_LIB( [cares], [ares_library_init] )

dnl Data structures
AC_CHECK_AAPL()

dnl Colm
AC_CHECK_COLM([0.13.0.4])


AC_SUBST(SED_SUBST)

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
