dnl ============================================================================
dnl ArangoDB                                                  (c) triAGENS GmbH
dnl ============================================================================

dnl ============================================================================
dnl --SECTION--                                  triAGENS GmbH Build Environment
dnl ============================================================================

AC_INIT([triAGENS ArangoDB], [1.3.devel], [info@triagens.de], [arangodb], [http://www.arangodb.org])

dnl ----------------------------------------------------------------------------
dnl auxillary directory for install-sh and missing
dnl ----------------------------------------------------------------------------

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

CURRENT_DIR=`pwd`
AC_MSG_NOTICE([configure started in '$CURRENT_DIR])
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


dnl ============================================================================
dnl --SECTION--                                              3RD-PARTY LIBRARIES
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl LIBEV
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-libev,
  AS_HELP_STRING([--enable-all-in-one-libev], [use supplied LIBEV from 3rdParty directory]),
  [tr_ALL_IN_ONE_LIBEV="${enableval:-yes}"],
  [tr_ALL_IN_ONE_LIBEV=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_LIBEV, test "x$tr_ALL_IN_ONE_LIBEV" = xyes)

dnl ----------------------------------------------------------------------------
dnl V8
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-v8,
  AS_HELP_STRING([--enable-all-in-one-v8], [use supplied V8 from 3rdParty directory]),
  [tr_ALL_IN_ONE_V8="${enableval:-yes}"],
  [tr_ALL_IN_ONE_V8=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_V8, test "x$tr_ALL_IN_ONE_V8" = xyes)

dnl ----------------------------------------------------------------------------
dnl ICU
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-icu,
  AS_HELP_STRING([--enable-all-in-one-icu], [use supplied ICU from 3rdParty directory (default: no)]),
  [tr_ALL_IN_ONE_ICU="${enableval:-yes}"],
  [tr_ALL_IN_ONE_ICU=no]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_ICU, test "x$tr_ALL_IN_ONE_ICU" = xyes)

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

m4_include([m4/configure.static])
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
dnl LIBEV
dnl ----------------------------------------------------------------------------

if test "x$tr_ALL_IN_ONE_LIBEV" = xyes;  then
  m4_include([m4/all-in-one.libev])
else
  m4_include([m4/external.libev])
fi

dnl ----------------------------------------------------------------------------
dnl MRUBY
dnl ----------------------------------------------------------------------------

m4_include([m4/all-in-one.mruby])

dnl ----------------------------------------------------------------------------
dnl V8
dnl ----------------------------------------------------------------------------

if test "x$tr_ALL_IN_ONE_V8" = xyes;  then
  m4_include([m4/all-in-one.v8])
else
  m4_include([m4/external.v8])
fi

dnl ----------------------------------------------------------------------------
dnl ICU
dnl ----------------------------------------------------------------------------

if test "x$tr_ALL_IN_ONE_ICU" = xyes;  then
  m4_include([m4/all-in-one.icu])
else
  m4_include([m4/external.icu])
fi


dnl ============================================================================
dnl --SECTION--                                                   GENERATE FILES
dnl ============================================================================

BUILD_H="\$(top_srcdir)/build_posix.h"
AC_SUBST(BUILD_H)

AC_CONFIG_FILES([Makefile Documentation/arango.template])
AC_CONFIG_HEADER([config/config.h lib/BasicsC/local-configuration.h])
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
