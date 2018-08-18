AC_INIT([updown], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/common.m4])

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

AC_CHECK_KRING
AC_CHECK_SHUTTLE

dnl
dnl Broker
dnl

AC_ARG_WITH([broker],
	[AC_HELP_STRING([--with-broker], [location of brokerj install])],
	[BROKER_PREFIX="$withval"], [BROKER_PREFIX="$DEPS"])

AC_CHECK_FILES( [$BROKER_PREFIX/bin/broker],
	[], [AC_ERROR([broker is required to build this package])])

SED_SUBST="$SED_SUBST -e 's|[@]BROKER_PREFIX[@]|$BROKER_PREFIX|g'"
AC_SUBST(BROKER_PREFIX)

dnl
dnl Net Parser
dnl

AC_ARG_WITH([netp],
	[AC_HELP_STRING([--with-netp], [location of brokerj install])],
	[NETP_PREFIX="$withval"], [NETP_PREFIX="$DEPS"])

AC_CHECK_FILES( [$NETP_PREFIX/bin/netp],
	[], [AC_ERROR([netp is required to build this package])])

SED_SUBST="$SED_SUBST -e 's|[@]NETP_PREFIX[@]|$NETP_PREFIX|g'"
AC_SUBST(NETP_PREFIX)

dnl
dnl TLS Proxy
dnl

AC_ARG_WITH([tlsproxy],
	[AC_HELP_STRING([--with-tlsproxy], [location of tlsproxy install])],
	[TLSPROXY_PREFIX="$withval"], [TLSPROXY_PREFIX="$DEPS"])

AC_CHECK_FILES( [$TLSPROXY_PREFIX/bin/tlsproxy],
	[], [AC_ERROR([tlsproxy is required to build this package])])

SED_SUBST="$SED_SUBST -e 's|[@]TLSPROXY_PREFIX[@]|$TLSPROXY_PREFIX|g'"
AC_SUBST(TLSPROXY_PREFIX)

dnl
dnl Fetch
dnl

AC_ARG_WITH([fetch],
	[AC_HELP_STRING([--with-fetch], [location of fetch install])],
	[FETCH_PREFIX="$withval"], [FETCH_PREFIX="$DEPS"])

AC_CHECK_FILES( [$FETCH_PREFIX/bin/fetch],
	[], [AC_ERROR([fetch is required to build this package])])

SED_SUBST="$SED_SUBST -e 's|[@]FETCH_PREFIX[@]|$FETCH_PREFIX|g'"
AC_SUBST(FETCH_PREFIX)

dnl
dnl Trader
dnl

AC_ARG_WITH([trader],
	[AC_HELP_STRING([--with-trader], [location of trader install])],
	[TRADER_PREFIX="$withval"], [TRADER_PREFIX="$DEPS"])

AC_CHECK_FILES( [$TRADER_PREFIX/bin/trader],
	[], [AC_ERROR([trader is required to build this package])])

SED_SUBST="$SED_SUBST -e 's|[@]TRADER_PREFIX[@]|$TRADER_PREFIX|g'"
AC_SUBST(TRADER_PREFIX)

dnl
dnl pipeline DB
dnl
AC_CHECK_PIPELINE


dnl
dnl Done
dnl

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
