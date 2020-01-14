dnl
dnl Common substitutions. Can only put pre-defined (in a shell sense)
dnl variables. Anything defined below must be added to SED_SUBST.
dnl
SED_SUBST="\
	-e 's|[@]PACKAGE[@]|${PACKAGE}|g' \
	-e 's|[@]PACKAGE_NAME[@]|${PACKAGE_NAME}|g' \
	-e 's|[@]PACKAGE_VERSION[@]|${PACKAGE_VERSION}|g' \
	-e 's|[@]prefix[@]|${prefix}|g' \
	-e 's|[@]bindir[@]|${bindir}|g' \
	-e 's|[@]localstatedir[@]|${localstatedir}|g' \
	-e 's|[@]libexecdir[@]|${libexecdir}|g' \
	-e 's|[@]sysconfdir[@]|${sysconfdir}|g' \
	-e 's|[@]datadir[@]|${datadir}|g' \
"

dnl
dnl pkgdatadir
dnl
pkgdatadir='${datadir}/${PACKAGE_NAME}'
AC_SUBST(pkgdatadir)
SED_SUBST="$SED_SUBST -e 's|[@]pkgdatadir[@]|$pkgdatadir|g'"

dnl
dnl pkgstatedir
dnl
pkgstatedir='${localstatedir}/${PACKAGE_NAME}'
AC_SUBST(pkgstatedir)
SED_SUBST="$SED_SUBST -e 's|[@]pkgstatedir[@]|$pkgstatedir|g'"

dnl
dnl pkglibexecdir
dnl
pkglibexecdir='${libexecdir}/${PACKAGE_NAME}'
AC_SUBST(pkglibexecdir)
SED_SUBST="$SED_SUBST -e 's|[@]pkglibexecdir[@]|$pkglibexecdir|g'"

dnl
dnl logdir
dnl
logdir='${localstatedir}/log'
AC_SUBST(logdir)
SED_SUBST="$SED_SUBST -e 's|[@]logdir[@]|$logdir|g'"

dnl
dnl piddir
dnl
piddir='${localstatedir}/run'
AC_SUBST(piddir)
SED_SUBST="$SED_SUBST -e 's|[@]piddir[@]|$piddir|g'"

dnl
dnl (netcol) modulesdir
dnl
modulesdir='${pkgdatadir}/modules'
AC_SUBST(modulesdir)
SED_SUBST="$SED_SUBST -e 's|[@]modulesdir[@]|$modulesdir|g'"

dnl
dnl kmoddir
dnl
dnl If we just use libdir for kernel modules we will get errors from automake
dnl either about the wrong type of thing, or the name being nonstandard.
dnl With this we can use "kmod_DATA = something.ko"
dnl
kmoddir='${libdir}'
AC_SUBST(kmoddir)
SED_SUBST="$SED_SUBST -e 's|[@]kmoddir[@]|$kmoddir|g'"

dnl
dnl Generic dependency specification. If a --with-<PACKAGE> option is not given,
dnl the --with-deps location is used. If the generic form not given, we default to
dnl /opt/colm.
dnl

AC_ARG_WITH(deps,
	[AC_HELP_STRING([--with-deps], [generic dependency location])],
	[DEPS="$withval"],
	[DEPS="/opt/colm"]
)

dnl
dnl Aapl Library Check
dnl
AC_DEFUN([AC_CHECK_AAPL], [
	AC_ARG_WITH(aapl,
		[AC_HELP_STRING([--with-aapl], [location of aapl install])],
		[
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			WITH_AAPL=$withval
		],
		[
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			WITH_AAPL=$DEPS
		]
	)

	AC_CHECK_HEADER(
		[aapl/avltree.h],
		[],
		[AC_ERROR([aapl is required to build this package])]
	)

	AC_SUBST(GENF)
	AC_SUBST(WITH_AAPL)
])


dnl
dnl Ragel Check
dnl
AC_DEFUN([AC_CHECK_RAGEL], [
	EXPECTED_VER=$1
	if test -z "$EXPECTED_VER"; then
		AC_ERROR( [check ragel: expected version not passed in] )
	fi

	AC_ARG_WITH(ragel,
		[AC_HELP_STRING([--with-ragel], [location of ragel install])],
		[RAGEL="$withval/bin/ragel"],
		[RAGEL="$DEPS/bin/ragel"]
	)

	AC_CHECK_FILES(
		[$RAGEL],
		[],
		[AC_ERROR([ragel is required to build this package])]
	)
	AC_SUBST(RAGEL)

	INSTALLED_VER=`$RAGEL -v | sed -n '1{s/^.*version //; s/ .*$//; p}'`
	if test "x$INSTALLED_VER" != "x$EXPECTED_VER"; then
		AC_ERROR( [check ragel: expected version $EXPECTED_VER, but $INSTALLED_VER is installed] )
	fi
])

dnl
dnl Colm Programming Language Check
dnl
AC_DEFUN([AC_CHECK_COLM], [
	EXPECTED_VER=$1
	if test -z "$EXPECTED_VER"; then
		AC_ERROR( [check colm: expected version not passed in] )
	fi

	AC_ARG_WITH(colm,
		[AC_HELP_STRING([--with-colm], [location of colm install])],
		[
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="-L$withval/lib ${LDFLAGS}"
			COLM="$withval/bin/colm"
		],
		[
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib"
			COLM="$DEPS/bin/colm"
		]
	)

	AC_CHECK_FILES(
		[$COLM],
		[],
		[AC_ERROR([colm is required to build this package])]
	)
	AC_SUBST(COLM)

	INSTALLED_VER=`$COLM -v | sed -n '1{s/^.*version //; s/ .*$//; p}'`
	if test "x$INSTALLED_VER" != "x$EXPECTED_VER"; then
		AC_ERROR( [check colm: expected version $EXPECTED_VER, but $INSTALLED_VER is installed] )
	fi
])

dnl
dnl Things that genf programs need. Used by the genf package itself, as well as
dnl the test suite.
dnl
AC_DEFUN([AC_CHECK_GENF_DEPS], [

	dnl pthread
	AC_CHECK_HEADER([pthread.h], [], [AC_ERROR([check genf: pthread.h not found])])
	AC_CHECK_LIB([pthread], [pthread_create], [], [AC_ERROR([check genf: cannot link with -lpthread])])

	dnl openssl
	AC_CHECK_HEADER([openssl/ssl.h], [], [AC_ERROR([check genf: openssl/openssl.h not found])])
	AC_CHECK_LIB([ssl], [SSL_library_init], [], [AC_ERROR([check genf: cannot link with -lssl])])
	AC_CHECK_LIB([crypto], [CRYPTO_thread_id], [], [AC_ERROR([check genf: cannot link with -lcrypto])])

	dnl c-ares (dns resolving)
	AC_CHECK_HEADER([ares.h], [], [AC_ERROR([check genf: ares.h not found])])
	AC_CHECK_LIB([cares], [ares_library_init], [], [AC_ERROR([check genf: cannot link with -lcares])])

	AC_CHECK_AAPL()
])

dnl
dnl genf code generator check
dnl
AC_DEFUN([AC_CHECK_GENF], [
	EXPECTED_VER=$1
	if test -z "$EXPECTED_VER"; then
		AC_ERROR( [check genf: expected version not passed in] )
	fi

	AC_ARG_WITH(genf,
		[AC_HELP_STRING([--with-genf], [location of genf install])],
		[
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="-L$withval/lib ${LDFLAGS}"
			GENF="$withval/bin/genf"
			WITH_AAPL_FN=$withval/share/genf/with-aapl
		],
		[
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib"
			GENF="$DEPS/bin/genf"
			WITH_AAPL_FN=$DEPS/share/genf/with-aapl
		]
	)

	if test -f $WITH_AAPL_FN; then
		WITH_AAPL_PATH=`cat $WITH_AAPL_FN`
		CPPFLAGS="${CPPFLAGS} -I$WITH_AAPL_PATH/include"
	fi

	dnl Version check disabled
	dnl	INSTALLED_VER=`$GENF -v | sed -n '1{s/^.*version //; s/ .*$//; p}'`
	dnl	if test "x$INSTALLED_VER" != "x$EXPECTED_VER"; then
	dnl		AC_ERROR( [check genf: expected version $EXPECTED_VER, but $INSTALLED_VER is installed] )
	dnl	fi

		
	dnl
	dnl Things that genf programs need
	dnl
	AC_CHECK_GENF_DEPS

	dnl GENF header and lib
	AC_CHECK_HEADER([genf/thread.h], [], [AC_ERROR([check genf: main header thread.h not found])])
	AC_CHECK_LIB([genf], [genf_thread_start], [], [AC_ERROR([check genf: cannot link with -lgenf])])

	dnl GENF binary
	AC_CHECK_FILES([$GENF], [], [AC_ERROR([check genf: could not find genf binary])])
	AC_SUBST(GENF)
])

dnl
dnl KRING check
dnl
AC_DEFUN([AC_CHECK_KRING], [
	AC_ARG_WITH(kring,
		[AC_HELP_STRING([--with-kring], [location of kring install])],
		[
			KRING_MOD="$withval/lib/kring.ko"
			KRING_SYMVERS="$withval/share/netcol/Module.symvers"
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="-L$withval/lib ${LDFLAGS}"
		],
		[
			KRING_MOD="$DEPS/lib/kring.ko"
			KRING_SYMVERS="$DEPS/share/netcol/Module.symvers"
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib"
		]
	)

	AC_CHECK_FILES( [$KRING_MOD], [],
			[AC_ERROR([kring is required to build this module])] )
	AC_CHECK_FILES( [$KRING_SYMVERS], [],
			[AC_ERROR([kring symvers not found in kring pkg])] )

	AC_CHECK_HEADER([kring/kring.h], [], [AC_ERROR([check kring: header kring.h not found])])
	AC_CHECK_LIB([kring], [kring_open], [],
			[AC_ERROR([check kring: cannot link with -lkring])])

	SED_SUBST="$SED_SUBST -e 's|[@]KRING_MOD[@]|$KRING_MOD|g'"

	AC_SUBST(KRING_SYMVERS)
	AC_SUBST(KRING_MOD)
])

dnl
dnl SHUTTLE check
dnl
AC_DEFUN([AC_CHECK_SHUTTLE], [
	AC_ARG_WITH(shuttle,
		[AC_HELP_STRING([--with-shuttle], [location of shuttle install])],
		[
			SHUTTLE_MOD="$withval/lib/shuttle.ko"
			SHUTTLE_SYMVERS="$withval/share/netcol/Module.symvers"
		],
		[
			SHUTTLE_MOD="$DEPS/lib/shuttle.ko"
			SHUTTLE_SYMVERS="$DEPS/share/netcol/Module.symvers"
		]
	)

	AC_CHECK_FILES( [$SHUTTLE_MOD], [],
			[AC_ERROR([shuttle is required to build this module])] )
	AC_CHECK_FILES( [$SHUTTLE_SYMVERS], [],
			[AC_ERROR([shuttle symvers not found in shuttle pkg])] )

	SED_SUBST="$SED_SUBST -e 's|[@]SHUTTLE_MOD[@]|$SHUTTLE_MOD|g'"

	AC_SUBST(SHUTTLE_SYMVERS)
	AC_SUBST(SHUTTLE_MOD)
])


dnl
dnl NETP check
dnl
AC_DEFUN([AC_CHECK_NETP], [
	AC_ARG_WITH(netp,
		[AC_HELP_STRING([--with-netp], [location of netp install])],
		[
			NETP_GENF="$withval/share/netcol/netp.gf"
			GENFFLAGS="${GENFFLAGS} -I$withval/share/netcol"
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="-L$withval/lib ${LDFLAGS}"
		],
		[
			NETP_GENF="$DEPS/share/netcol/netp.gf"
			GENFFLAGS="${GENFFLAGS} -I$DEPS/share/netcol"
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib"
		]
	)

	AC_CHECK_FILES( [$NETP_GENF], [],
			[AC_ERROR([netp is required to build this module])] )

	AC_CHECK_HEADER( [netp/netp.h], [], [AC_ERROR([netp/netp.h not found])] )
	AC_CHECK_LIB([netp], [netp_open], [], [AC_ERROR([check netp: cannot link with -lnetp])])

	AC_SUBST(NETP_GENF)
	AC_SUBST(GENFFLAGS)
])

dnl
dnl BROKER check
dnl
AC_DEFUN([AC_CHECK_BROKER], [
	AC_ARG_WITH(broker,
		[AC_HELP_STRING([--with-broker], [location of broker install])],
		[
			BROKER_GENF="$withval/share/netcol/broker.gf"
			GENFFLAGS="${GENFFLAGS} -I$withval/share/netcol"
		],
		[
			BROKER_GENF="$DEPS/share/netcol/broker.gf"
			GENFFLAGS="${GENFFLAGS} -I$DEPS/share/netcol"
		]
	)

	AC_CHECK_FILES( [$BROKER_GENF], [],
			[AC_ERROR([broker is required to build this module])] )

	AC_SUBST(BROKER_GENF)
	AC_SUBST(GENFFLAGS)
])

dnl
dnl SCHEMA check
dnl
AC_DEFUN([AC_CHECK_SCHEMA], [
	AC_ARG_WITH(schema,
		[AC_HELP_STRING([--with-schema], [location of schema install])],
		[
			SCHEMA_GENF="$withval/share/schema/schema.gf"
			GENFFLAGS="${GENFFLAGS} -I$withval/share/schema"
		],
		[
			SCHEMA_GENF="$DEPS/share/schema/schema.gf"
			GENFFLAGS="${GENFFLAGS} -I$DEPS/share/schema"
		]
	)

	AC_CHECK_FILES( [$SCHEMA_GENF], [],
			[AC_ERROR([schema is required to build this module])] )

	AC_SUBST(SCHEMA_GENF)
	AC_SUBST(GENFFLAGS)
])

dnl
dnl MODULE check
dnl
AC_DEFUN([AC_CHECK_MODULE], [
	AC_ARG_WITH(module,
		[AC_HELP_STRING([--with-module], [location of module install])],
		[
			MODULE_GENF="$withval/share/module/module.gf"
			GENFFLAGS="${GENFFLAGS} -I$withval/module/schema"
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="-L$withval/lib ${LDFLAGS}"
		],
		[
			MODULE_GENF="$DEPS/share/module/module.gf"
			GENFFLAGS="${GENFFLAGS} -I$DEPS/share/module"
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib"
		]
	)

	AC_CHECK_FILES( [$MODULE_GENF], [],
			[AC_ERROR([module package is required to build this module])] )

	AC_CHECK_HEADER( [module/module.h], [], [AC_ERROR([module/module.h not found])] )
	AC_CHECK_LIB([module], [module_open], [], [AC_ERROR([check netp: cannot link with -lmodule])])

	AC_SUBST(MODULE_GENF)
	AC_SUBST(GENFFLAGS)
])


if uname -r | grep -q '^4\.10\.0-'; then
	KERNVER=`uname -r`;
	MKERNVER=4
fi

if uname -r | grep -q '^4\.4\.0-'; then
	KERNVER=`uname -r`;
	MKERNVER=4
fi

if uname -r | grep -q '^3\.13\.0-'; then
	KERNVER=`uname -r`;
	MKERNVER=3
fi

KERNDIR=/lib/modules/$KERNVER/build
AC_DEFINE_UNQUOTED([KERNVER], [$MKERNVER], [major kernel version])

AC_DEFUN([AC_CHECK_KERNDIR], [
	AC_CHECK_FILE( [$KERNDIR/Module.symvers], [],
		[AC_ERROR([could not locate kernel build dir at $KERNDIR] )] )
	AC_SUBST([KERNDIR])
])

dnl
dnl user to run as
dnl
AC_ARG_WITH(run-as,
	[AC_HELP_STRING([--with-run-as], [user to run as when started as root])],
	[RUN_AS="$withval"],
	[RUN_AS=`whoami`]
)
AC_SUBST(RUN_AS)
SED_SUBST="$SED_SUBST -e 's|[@]RUN_AS[@]|$RUN_AS|g'"

dnl
dnl Check for libcap. Require this in all applications dropping root and keeping necessary caps is built into genf.
dnl
AC_CHECK_LIB([cap], [cap_get_proc])
AC_CHECK_HEADER( [sys/capability.h], [], [AC_ERROR([sys/capability.h not found])] )

dnl
dnl Make a zlib check available.
dnl
AC_DEFUN([AC_CHECK_ZLIB], [
	AC_CHECK_HEADER([zlib.h], [], [AC_ERROR([unable to include zlib.h])])
	AC_CHECK_LIB([z], [inflate], [], [AC_ERROR([unable to link with -lz])])
])

dnl
dnl Make a brotli check available.
dnl
AC_DEFUN([AC_CHECK_BROTLIDEC], [
	AC_CHECK_HEADER([brotli/decode.h], [], [AC_ERROR([unable to include brotli/decode.h])])
	AC_CHECK_LIB([brotlidec], [BrotliDecoderCreateInstance], [], [AC_ERROR([unable to link with -lbrotlidec])])
])

dnl
dnl Make a sqlite3 check available.
dnl
AC_DEFUN([AC_CHECK_SQLITE3], [
	AC_CHECK_HEADER([sqlite3.h], [], [AC_ERROR([unable to include sqlite3.h])])
	AC_CHECK_LIB([sqlite3], [sqlite3_open], [], [AC_ERROR([unable to link with -lsqlite3])])
])

dnl
dnl Make a pipelinedb check available.
dnl
AC_DEFUN([AC_CHECK_PIPELINE], [
	AC_ARG_WITH(pipeline,
		[AC_HELP_STRING([--with-pipeline], [location of pipeline install])],
		[
			PIPELINE_PREFIX="$withval"
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="${LDFLAGS} -L$withval/lib -Wl,-rpath -Wl,$withval/lib"
		],
		[
			PIPELINE_PREFIX="$DEPS"
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib -Wl,-rpath -Wl,${DEPS}/lib"
		]
	)

	AC_CHECK_FILES(
		[$PIPELINE_PREFIX/libexec/pipeline/init.d],
		[],
		[AC_ERROR([pipeline DB is required to build this package])]
	)

	AC_CHECK_HEADER([libpq-fe.h], [], [AC_ERROR([unable to include libpq-fe.h])])
	AC_CHECK_LIB([pq], [PQconnectdb], [], [AC_ERROR([unable to link with -lpq])])

	SED_SUBST="$SED_SUBST -e 's|[@]PIPELINE_PREFIX[@]|$PIPELINE_PREFIX|g'"
	AC_SUBST(PIPELINE_PREFIX)
])

dnl
dnl Make a postgres check available.
dnl
AC_DEFUN([AC_CHECK_POSTGRES], [
	AC_ARG_WITH(postgres,
		[AC_HELP_STRING([--with-postgres], [location of postgres install])],
		[
			POSTGRES_PREFIX="$withval"
			CPPFLAGS="-I$withval/include ${CPPFLAGS}"
			LDFLAGS="${LDFLAGS} -L$withval/lib -Wl,-rpath -Wl,$withval/lib"
		],
		[
			POSTGRES_PREFIX="$DEPS"
			CPPFLAGS="${CPPFLAGS} -I$DEPS/include"
			LDFLAGS="${LDFLAGS} -L$DEPS/lib -Wl,-rpath -Wl,${DEPS}/lib"
		]
	)

	AC_CHECK_FILES(
		[$POSTGRES_PREFIX/libexec/postgres/init.d],
		[],
		[AC_ERROR([postgres DB is required to build this package])]
	)

	AC_CHECK_HEADER([libpq-fe.h], [], [AC_ERROR([unable to include libpq-fe.h])])
	AC_CHECK_LIB([pq], [PQconnectdb], [], [AC_ERROR([unable to link with -lpq])])

	SED_SUBST="$SED_SUBST -e 's|[@]POSTGRES_PREFIX[@]|$POSTGRES_PREFIX|g'"
	AC_SUBST(POSTGRES_PREFIX)
])

AC_SUBST(SED_SUBST)
