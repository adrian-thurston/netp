AC_INIT([updown], [0.1.0], [thurston@colm.net])
AM_INIT_AUTOMAKE()

AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/common.m4])

AC_CHECK_KRING
AC_CHECK_SHUTTLE

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

AC_CONFIG_HEADERS(src/config.h)
AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
