AC_INIT([netcol], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/common.m4])

: ${CFLAGS="-Wall -g"}
: ${CXXFLAGS="-Wall -g"}

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_LANG_CPLUSPLUS

dnl zlib and brotli
AC_CHECK_ZLIB()
AC_CHECK_BROTLIDEC()

dnl Colm
AC_CHECK_COLM([0.14])

dnl ragel
AC_CHECK_RAGEL([7.0])

dnl Usage dependencies are stored in pkgbuild/common.m4
AC_CHECK_GENF_DEPS()

AC_CHECK_FUNC([X509_check_host],
	[AC_DEFINE([HAVE_X509_CHECK_HOST], [1], [define if X509_check_host exists])],
	dnl	[AC_ERROR([X509_check_host not found])]
)

# For taking in config.h
CPPFLAGS="-I\$(abs_top_srcdir)/src ${CPPFLAGS}"

dnl For building against GENF.
CPPFLAGS="-I\$(abs_top_srcdir)/genf/include ${CPPFLAGS}"
LDFLAGS="-L\$(abs_top_builddir)/genf ${LDFLAGS}"
GENF="\$(abs_top_builddir)/genf/genf"

AC_CHECK_KERNDIR

dnl For building against kring and bringing up/down.
KRING_MOD="\$(abs_top_builddir)/kring/kring.ko"
KRING_SYMVERS="\$(abs_top_builddir)/kring/Module.symvers"
CPPFLAGS="-I\$(abs_top_builddir)/kring/include ${CPPFLAGS}"
LDFLAGS="-L\$(abs_top_builddir)/kring ${LDFLAGS}"
SED_SUBST="$SED_SUBST -e 's|[@]KRING_MOD[@]|$KRING_MOD|g'"

dnl For bringing shuttle up/down.
SHUTTLE_MOD="\$(abs_top_builddir)/shuttle/shuttle.ko"
SHUTTLE_SYMVERS="\$(abs_top_builddir)/shuttle/Module.symvers"
SED_SUBST="$SED_SUBST -e 's|[@]SHUTTLE_MOD[@]|$SHUTTLE_MOD|g'"

dnl For building against netp.
NETP_GENF="\$(abs_top_srcdir)/netp/netp.gf"
GENFFLAGS="${GENFFLAGS} -I\$(abs_top_srcdir)/netp"
CPPFLAGS="-I\$(abs_top_srcdir)/netp/include ${CPPFLAGS}"
LDFLAGS="-L\$(abs_top_srcdir)/netp ${LDFLAGS}"

dnl
dnl OpenVPN support
dnl
DH_PATH=/usr/share/doc/openvpn/examples/sample-keys
DH_SEL=""
AC_CHECK_FILE( [$DH_PATH/dh1024.pem], [DH_SEL=dh1024],
	[AC_CHECK_FILE( [$DH_PATH/dh2048.pem], [DH_SEL=dh2048],
		[AC_ERROR([could not find DH keys file])])]
)

if test -n "$DH_SEL"; then
	DH_KEYS=$DH_PATH/$DH_SEL.pem
	SED_SUBST="$SED_SUBST -e 's|[@]DH_KEYS[@]|$DH_KEYS|g'"
	AC_SUBST(DH_KEYS)
fi

BROKER_BIN="\$(abs_top_builddir)/broker/broker"
SNIFF_BIN="\$(abs_top_builddir)/sniff/sniff"
TLSPROXY_BIN="\$(abs_top_builddir)/tlsproxy/tlsproxy"
FETCH_BIN="\$(abs_top_builddir)/fetch/fetch"

SED_SUBST="$SED_SUBST -e 's|[@]BROKER_BIN[@]|$BROKER_BIN|g'"
SED_SUBST="$SED_SUBST -e 's|[@]SNIFF_BIN[@]|$SNIFF_BIN|g'"
SED_SUBST="$SED_SUBST -e 's|[@]TLSPROXY_BIN[@]|$TLSPROXY_BIN|g'"
SED_SUBST="$SED_SUBST -e 's|[@]FETCH_BIN[@]|$FETCH_BIN|g'"

AC_SUBST(GENF)
AC_SUBST(GENFFLAGS)
AC_SUBST(KRING_SYMVERS)
AC_SUBST(KRING_MOD)
AC_SUBST(SED_SUBST)

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	genf/Makefile
	kring/Makefile
	shuttle/Makefile
	netp/Makefile
	broker/Makefile
	sniff/Makefile
	tlsproxy/Makefile
	fetch/Makefile
	updown/Makefile
	test/Makefile
	test/describe1/Makefile
])

AC_OUTPUT
