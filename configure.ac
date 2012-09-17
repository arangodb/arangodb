dnl ============================================================================
dnl ArangoDB                                                  (c) triAGENS GmbH
dnl ============================================================================

dnl ============================================================================
dnl --SECTION--                                  triAGENS GmbH Build Environment
dnl ============================================================================

AC_INIT([triAGENS ArangoDB], [1.0.0], [info@triagens.de], [arangodb], [http://www.arangodb.org])

dnl ----------------------------------------------------------------------------
dnl auxillary directory for install-sh and missing
dnl ----------------------------------------------------------------------------

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

dnl ----------------------------------------------------------------------------
dnl ERRORS
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(errors-dependency,
  AS_HELP_STRING([--enable-errors-dependency], [generate dependencies for error files]),
  [tr_ERRORS_DEPENDENCY="${enableval:-yes}"],
  [tr_ERRORS_DEPENDENCY=no]
)

AM_CONDITIONAL(ENABLE_ERRORS_DEPENDENCY, test "x$tr_ERRORS_DEPENDENCY" = xyes)

dnl ============================================================================
dnl --SECTION--                                              3RD-PARTY LIBRARIES
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl BOOST TEST
dnl ----------------------------------------------------------------------------

AC_ARG_WITH([boost-test],
  AS_HELP_STRING([--with-boost-test], [use boost unit test framework]),
  [tr_BOOST_TEST=yes],
  [tr_BOOST_TEST=no])

AM_CONDITIONAL(ENABLE_BOOST_TEST, test "x$tr_BOOST_TEST" = xyes)

dnl ----------------------------------------------------------------------------
dnl BOOST
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-boost,
  AS_HELP_STRING([--enable-all-in-one-boost], [use supplied BOOST from 3rdParty directory]),
  [tr_ALL_IN_ONE_BOOST="${enableval:-yes}"],
  [tr_ALL_IN_ONE_BOOST=yes]
)

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
dnl PROTOBUF
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-protobuf,
  AS_HELP_STRING([--enable-all-in-one-protobuf], [use supplied PROTOBUF from 3rdParty directory]),
  [tr_ALL_IN_ONE_PROTOBUF="${enableval:-yes}"],
  [tr_ALL_IN_ONE_PROTOBUF=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_PROTOBUF, test "x$tr_ALL_IN_ONE_PROTOBUF" = xyes)

dnl ----------------------------------------------------------------------------
dnl V8
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-v8,
  AS_HELP_STRING([--enable-all-in-one-v8], [use supplied V8 from 3rdParty directory]),
  [tr_ALL_IN_ONE_V8="${enableval:-yes}"],
  [tr_ALL_IN_ONE_V8=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_V8, test "x$tr_ALL_IN_ONE_V8" = xyes)

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
m4_include([m4/configure.memory])
m4_include([m4/configure.logging])
m4_include([m4/configure.largefile])
m4_include([m4/configure.threads])
m4_include([m4/configure.documentation])
m4_include([m4/configure.coverage])
m4_include([m4/configure.flex])
m4_include([m4/configure.bison])

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
dnl BOOST
dnl ----------------------------------------------------------------------------

if test "x$tr_ALL_IN_ONE_BOOST" = xyes;  then
  m4_include([m4/all-in-one.boost])
else
  m4_include([m4/external.boost])
fi

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
dnl PROTOBUF
dnl ----------------------------------------------------------------------------

if test "x$tr_ALL_IN_ONE_PROTOBUF" = xyes;  then
  m4_include([m4/all-in-one.protobuf])
else
  m4_include([m4/external.protobuf])
fi

dnl ----------------------------------------------------------------------------
dnl V8
dnl ----------------------------------------------------------------------------

if test "x$tr_ALL_IN_ONE_V8" = xyes;  then
  m4_include([m4/all-in-one.v8])
else
  m4_include([m4/external.v8])
fi

dnl ----------------------------------------------------------------------------
dnl ZEROMQ
dnl ----------------------------------------------------------------------------

m4_include([m4/all-in-one.zeromq])

dnl ----------------------------------------------------------------------------
dnl ICU
dnl ----------------------------------------------------------------------------

m4_include([m4/external.icu])

dnl ============================================================================
dnl --SECTION--                                                   GENERATE FILES
dnl ============================================================================

BUILD_H="\$(top_srcdir)/build.h"
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
