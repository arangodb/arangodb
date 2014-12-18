dnl ============================================================================
dnl ArangoDB                                                  (c) triAGENS GmbH
dnl ============================================================================

dnl ============================================================================
dnl --SECTION--                                  triAGENS GmbH Build Environment
dnl ============================================================================

AC_INIT([triAGENS ArangoDB], [2.4.0-devel], [info@arangodb.com], [arangodb], [https://www.arangodb.com])

dnl ----------------------------------------------------------------------------
dnl auxillary directory for install-sh and missing
dnl ----------------------------------------------------------------------------

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

CURRENT_DIR=`pwd`

AC_MSG_NOTICE([configure started in '$CURRENT_DIR])
AC_MSG_NOTICE([with CC='$CC'])
AC_MSG_NOTICE([with CXX='$CXX'])
AC_MSG_NOTICE([with CPPFLAGS='$CPPFLAGS'])
AC_MSG_NOTICE([with CFLAGS='$CFLAGS'])
AC_MSG_NOTICE([with CXXFLAGS='$CXXFLAGS'])
AC_MSG_NOTICE([with LDFLAGS='$LDFLAGS'])


dnl ----------------------------------------------------------------------------
dnl grab the configure command, options and flags and store them in m4 variables
dnl ----------------------------------------------------------------------------

if test `expr -- [$]0 : "'.*"` = 0; then
  TRI_CONFIGURE_COMMAND="$TRI_CONFIGURE_COMMAND '[$]0'"
else 
  TRI_CONFIGURE_COMMAND="$TRI_CONFIGURE_COMMAND [$]0"
fi

TRI_CONFIGURE_OPTIONS="$ac_configure_args"

for var in CFLAGS CXXFLAGS CPPFLAGS LDFLAGS EXTRA_LDFLAGS_PROGRAM LIBS CC CXX; do
  eval val=\$$var
  if test -n "$val"; then
    TRI_CONFIGURE_FLAGS="$TRI_CONFIGURE_FLAGS $var='$val'"
  fi
done

AC_SUBST(TRI_CONFIGURE_COMMAND)
AC_SUBST(TRI_CONFIGURE_OPTIONS)
AC_SUBST(TRI_CONFIGURE_FLAGS)

AC_DEFINE_UNQUOTED(TRI_CONFIGURE_COMMAND, "$TRI_CONFIGURE_COMMAND", "" , [configure command])
AC_DEFINE_UNQUOTED(TRI_CONFIGURE_OPTIONS, "$TRI_CONFIGURE_OPTIONS", "" , [configure options])
AC_DEFINE_UNQUOTED(TRI_CONFIGURE_FLAGS, "$TRI_CONFIGURE_FLAGS", "" , [configure flags])


dnl ----------------------------------------------------------------------------
dnl grab the git version number if available
dnl ----------------------------------------------------------------------------

if test -d ".git"; then
  TRI_REPOSITORY_VERSION=`git describe --all --tags --long --abbrev=40 --dirty="-dirty" 2> /dev/null`
else
  TRI_REPOSITORY_VERSION=""
fi

AC_SUBST(TRI_REPOSITORY_VERSION)
AC_DEFINE_UNQUOTED(TRI_REPOSITORY_VERSION, "$TRI_REPOSITORY_VERSION", "" , [repository version])


dnl ============================================================================
dnl --SECTION--                                              3RD-PARTY LIBRARIES
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl ETCD
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-etcd,
  AS_HELP_STRING([--enable-all-in-one-etcd], [use supplied etcd from 3rdParty directory (default: yes)]),
  [tr_ALL_IN_ONE_ETCD="${enableval:-yes}"],
  [tr_ALL_IN_ONE_ETCD=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_ETCD, test "x$tr_ALL_IN_ONE_ETCD" = xyes)

dnl ----------------------------------------------------------------------------
dnl GO
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(internal-go,
  AS_HELP_STRING([--enable-internal-go], [use go binaries from 3rdParty directory (default: no)]),
  [tr_INTERNAL_GO="${enableval:-yes}"],
  [tr_INTERNAL_GO=no]
)

AM_CONDITIONAL(ENABLE_INTERNAL_GO, test "x$tr_INTERNAL_GO" = xyes)

if test "x$tr_INTERNAL_GO" = "xno"; then
  AC_CHECK_PROG(GO, go, go)
  if test "x$GO" = "x"; then
    AC_MSG_WARN([no go executable found.])
    BASIC_INFO="$BASIC_INFO|GO BINARY: not found"
  else
    BASIC_INFO="$BASIC_INFO|GO BINARY: $GO"
  fi
fi

dnl ============================================================================
dnl --SECTION--                                                          OPTIONS
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl RELATIVE PATH
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(relative,
  AS_HELP_STRING([--enable-relative], [all path are relative to the binary (default: no)]),
  [tr_RELATIVE="${enableval:-yes}"],
  [tr_RELATIVE=no]
)

AM_CONDITIONAL(ENABLE_RELATIVE, test "x$tr_RELATIVE" = xyes)

if test "x$tr_RELATIVE" != xno;  then
  BASIC_INFO="$BASIC_INFO|RELATIVE PATHS: enabled"
else
  BASIC_INFO="$BASIC_INFO|RELATIVE PATHS: disabled"
fi

dnl ============================================================================
dnl --SECTION--                                                    CONFIGURATION
dnl ============================================================================

m4_include([m4/configure.basics])
AC_PROG_RANLIB

m4_include([m4/configure.logging])
m4_include([m4/configure.largefile])
m4_include([m4/configure.threads])
m4_include([m4/configure.documentation])
m4_include([m4/configure.coverage])
m4_include([m4/configure.maintainer])

dnl ============================================================================
dnl --SECTION--                                               EXTERNAL LIBRARIES
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl -lrt
dnl ----------------------------------------------------------------------------

m4_include([m4/clock_gettime])
AC_CLOCK()

dnl ----------------------------------------------------------------------------
dnl MATH
dnl ----------------------------------------------------------------------------

m4_include([m4/external.math])

dnl ----------------------------------------------------------------------------
dnl READLINE
dnl ----------------------------------------------------------------------------

m4_include([m4/external.readline])

dnl ----------------------------------------------------------------------------
dnl OPENSSL
dnl ----------------------------------------------------------------------------

m4_include([m4/external.openssl])

dnl ----------------------------------------------------------------------------
dnl MRUBY
dnl ----------------------------------------------------------------------------

MRUBY_CPPFLAGS=""
MRUBY_LDLAGS=""
MRUBY_LIBSLAGS=""

AC_ARG_ENABLE(mruby,
  AS_HELP_STRING([--enable-mruby], [enable MRUBY (default: no)]),
  [tr_MRUBY="${enableval:-yes}"],
  [tr_MRUBY=no]
)

if test "x$tr_MRUBY" = "xyes";  then
  MRUBY_VERSION="2013-08-22 (aa099f7f7d1f8c61db936fb7c39a3c734175af16)"

  MRUBY_CPPFLAGS="-I${srcdir}/3rdParty/mruby/include -DTRI_MRUBY_VERSION='\"${MRUBY_VERSION}\"'"
  MRUBY_LDFLAGS=""
  MRUBY_LIBS="${srcdir}/3rdParty/mruby/build/host/lib/libmruby.a"
fi

AM_CONDITIONAL(ENABLE_MRUBY, test "x$tr_MRUBY" = "xyes")

AC_SUBST(MRUBY_CPPFLAGS)
AC_SUBST(MRUBY_LDFLAGS)
AC_SUBST(MRUBY_LIBS)

if test "x$tr_MRUBY" = "xyes";  then
  LIB_INFO="$LIB_INFO|MRUBY: 3rdParty version"
  LIB_INFO="$LIB_INFO|MRUBY VERSION: ${TRI_MRUBY_VERSION}"

  LIB_INFO="$LIB_INFO|MRUBY_CPPFLAGS: ${MRUBY_CPPFLAGS}"
  LIB_INFO="$LIB_INFO|MRUBY_LDFLAGS:  ${MRUBY_LDFLAGS}"
  LIB_INFO="$LIB_INFO|MRUBY_LIBS:     ${MRUBY_LIBS}"
else
  LIB_INFO="$LIB_INFO|MRUBY: disabled"
fi

LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl V8
dnl ----------------------------------------------------------------------------

V8_VERSION="3.29.59"
V8_CPPFLAGS="-I${srcdir}/3rdParty/V8/include -DTRI_V8_VERSION='\"${V8_VERSION}\"'"
V8_LDFLAGS=""
V8_TARGET="native"

v8_lib_dir=""

if test x$tr_ARM == xyes;  then
  V8_TARGET="arm.release"
  v8_lib_dir="${srcdir}/3rdParty/V8/out/$V8_TARGET/obj.target/tools/gyp"

elif test x$tr_DARWIN == xyes;  then
  if test "x$tr_BITS" == x64;  then
    V8_TARGET="x64.release"
  else
    V8_TARGET="ia32.release"
  fi

  v8_lib_dir="${srcdir}/3rdParty/V8/out/$V8_TARGET"

else
  if test "x$tr_BITS" == x64;  then
    V8_TARGET="x64.release"
  else
    V8_TARGET="ia32.release"
  fi

  v8_lib_dir="${srcdir}/3rdParty/V8/out/$V8_TARGET/obj.target/tools/gyp"
fi

V8_LIBS="${v8_lib_dir}/libv8_base.a ${v8_lib_dir}/libv8_libbase.a ${v8_lib_dir}/libv8_libplatform.a ${v8_lib_dir}/libv8_nosnapshot.a"

if test "x$tr_FREEBSD" == xyes;  then
  V8_LIBS="$V8_LIBS -lexecinfo"
fi

AC_SUBST(V8_CPPFLAGS)
AC_SUBST(V8_LDFLAGS)
AC_SUBST(V8_LIBS)
AC_SUBST(V8_TARGET)

LIB_INFO="$LIB_INFO|V8: 3rdParty version"
LIB_INFO="$LIB_INFO|V8 VERSION: ${V8_VERSION}"

LIB_INFO="$LIB_INFO|V8_TARGET:   ${V8_TARGET}"

LIB_INFO="$LIB_INFO|V8_CPPFLAGS: ${V8_CPPFLAGS}"
LIB_INFO="$LIB_INFO|V8_LDFLAGS:  ${V8_LDFLAGS}"
LIB_INFO="$LIB_INFO|V8_LIBS:     ${V8_LIBS}"

LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl ICU
dnl ----------------------------------------------------------------------------

ICU_VERSION="52.2"
ICU_CPPFLAGS="-D_REENTRANT -I${srcdir}/3rdParty/V8/third_party/icu/source/common -I${srcdir}/3rdParty/V8/third_party/icu/source/i18n -I${srcdir}/3rdParty/V8/third_party/icu/source/io -DTRI_ICU_VERSION='\"${ICU_VERSION}\"'"
ICU_LDFLAGS=""

if test "x$tr_DARWIN" = xyes; then
  icu_lib_dir="${srcdir}/3rdParty/V8/out/$V8_TARGET"
else
  icu_lib_dir="${srcdir}/3rdParty/V8/out/$V8_TARGET/obj.target/third_party/icu"
fi

ICU_LIBS="-ldl -lm ${icu_lib_dir}/libicui18n.a ${icu_lib_dir}/libicuuc.a ${icu_lib_dir}/libicudata.a -ldl -lm"

AC_SUBST(ICU_CPPFLAGS)
AC_SUBST(ICU_LDFLAGS)
AC_SUBST(ICU_LIBS)

ICU_CPPFLAGS="${ICU_CPPFLAGS}"

LIB_INFO="$LIB_INFO|ICU: 3rdParty version"
LIB_INFO="$LIB_INFO|ICU VERSION: ${ICU_VERSION}"

LIB_INFO="$LIB_INFO|ICU_CPPFLAGS: ${ICU_CPPFLAGS}"
LIB_INFO="$LIB_INFO|ICU_LDFLAGS:  ${ICU_LDFLAGS}"
LIB_INFO="$LIB_INFO|ICU_LIBS:     ${ICU_LIBS}"

LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl LIBEV
dnl ----------------------------------------------------------------------------

LIBEV_VERSION="4.11"

LIBEV_CPPFLAGS="-I${srcdir}/3rdParty/libev -DTRI_LIBEV_VERSION='\"${LIBEV_VERSION}\"'"
LIBEV_CPPFLAGS="${LIBEV_CPPFLAGS}"
LIBEV_LDFLAGS=""
LIBEV_LIBS="${srcdir}/3rdParty/libev/BUILD/.libs/libev.a"

AC_SUBST(LIBEV_CPPFLAGS)
AC_SUBST(LIBEV_LDFLAGS)
AC_SUBST(LIBEV_LIBS)

LIB_INFO="$LIB_INFO|LIBEV: 3rdParty version"
LIB_INFO="$LIB_INFO|LIBEV VERSION: ${LIBEV_VERSION}"

LIB_INFO="$LIB_INFO|LIBEV_CPPFLAGS: ${LIBEV_CPPFLAGS}"
LIB_INFO="$LIB_INFO|LIBEV_LDFLAGS:  ${LIBEV_LDFLAGS}"
LIB_INFO="$LIB_INFO|LIBEV_LIBS:     ${LIBEV_LIBS}"

LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl ZLIB
dnl ----------------------------------------------------------------------------

ZLIB_VERSION="1.2.7"

ZLIB_CPPFLAGS=""
ZLIB_LDFLAGS=""
ZLIB_LIBS="${srcdir}/3rdParty/zlib-${ZLIB_VERSION}/libz.a"

AC_SUBST(ZLIB_CPPFLAGS)
AC_SUBST(ZLIB_LDFLAGS)
AC_SUBST(ZLIB_LIBS)

LIB_INFO="$LIB_INFO|ZLIB: 3rdParty version"
LIB_INFO="$LIB_INFO|ZLIB VERSION: ${ZLIB_VERSION}"

LIB_INFO="$LIB_INFO|ZLIB_CPPFLAGS: ${ZLIB_CPPFLAGS}"
LIB_INFO="$LIB_INFO|ZLIB_LDFLAGS:  ${ZLIB_LDFLAGS}"
LIB_INFO="$LIB_INFO|ZLIB_LIBS:     ${ZLIB_LIBS}"

dnl ============================================================================
dnl --SECTION--                                                   GENERATE FILES
dnl ============================================================================

BUILD_H="\$(top_srcdir)/build.h"
AC_SUBST(BUILD_H)

AC_CONFIG_FILES([Makefile Installation/MacOSX/Bundle/Info.plist])
AC_CONFIG_HEADER([config/config.h lib/Basics/local-configuration.h])
AC_OUTPUT

dnl ============================================================================
dnl --SECTION--                                                    GENERATE INFO
dnl ============================================================================

m4_include([m4/configure.information])

dnl ============================================================================
dnl --SECTION--                                                      END-OF-FILE
dnl ============================================================================

dnl Local Variables:
dnl mode: outline-minor
dnl outline-regexp: "^\\(dnl [a-zA-Z]\\|dnl --SECTION--\\)"
dnl End:
