dnl ============================================================================
dnl AvocadoDB                                                  (c) triAGENS GmbH
dnl ============================================================================

dnl ============================================================================
dnl PREAMBLE                                     triAGENS GmbH Build Environment
dnl ============================================================================

AC_INIT([triAGENS AvocadoDB], [0.1.0], [info@triagens.de], [avocado], [http://www.worldofvoc.com])

dnl ----------------------------------------------------------------------------
dnl auxillary directory for install-sh and missing
dnl ----------------------------------------------------------------------------

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

dnl ============================================================================
dnl OPTIONS                                        BISON, FLEX, ALL-IN-ONE, PATH
dnl ============================================================================

AC_ARG_ENABLE(bison,
  AS_HELP_STRING([--enable-bison], [enable BISON]),
  [tr_BISON="${enableval:-yes}"],
  [tr_BISON=no]
)

AM_CONDITIONAL(ENABLE_BISON, test "x$tr_BISON" = xyes)

AC_ARG_ENABLE(flex,
  AS_HELP_STRING([--enable-flex], [enable FLEX]),
  [tr_FLEX="${enableval:-yes}"],
  [tr_FLEX=no]
)

AM_CONDITIONAL(ENABLE_FLEX, test "x$tr_FLEX" = xyes)

AC_ARG_ENABLE(all-in-one,
  AS_HELP_STRING([--enable-all-in-one], [enable supplied V8, BOOST, LIBEV]),
  [tr_ALL_IN_ONE="${enableval:-yes}"],
  [tr_ALL_IN_ONE=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE, test "x$tr_ALL_IN_ONE" = xyes)

AC_ARG_ENABLE(install-dbdir,
  AS_HELP_STRING([--enable-install-dbdir], [install an empty database directory]),
  [tr_DBDIR="${enableval:-yes}"],
  [tr_DBDIR=yes]
)

AM_CONDITIONAL(ENABLE_INSTALL_DBDIR, test "x$tr_DBDIR" = xyes)

AC_ARG_ENABLE(relative,
  AS_HELP_STRING([--enable-relative], [all path are relative to the binary (yes/no/devel [default])]),
  [tr_RELATIVE="${enableval:-devel}"],
  [tr_RELATIVE=devel]
)

if test "x$tr_RELATIVE" = xyes;  then
  AC_DEFINE_UNQUOTED(TRI_ENABLE_RELATIVE_SYSTEM, 1, [true if relative system paths should be used])
else
if test "x$tr_RELATIVE" = xdevel;  then
  AC_DEFINE_UNQUOTED(TRI_ENABLE_RELATIVE_DEVEL, 1, [true if relative development paths should be used])
fi
fi

dnl ============================================================================
dnl CHECKS                                 HOST, PROGRAMS, COMPILER, LIB, SYSTEM
dnl ============================================================================

m4_include([m4/configure.basics])
AC_PROG_RANLIB

m4_include([m4/configure.static])
m4_include([m4/configure.32bit])
m4_include([m4/configure.logging])
m4_include([m4/configure.largefile])
m4_include([m4/configure.threads])
m4_include([m4/configure.dot])

if test "x$tr_FLEX" = xyes;  then
  m4_include([m4/configure.flex])
  BASIC_INFO="$BASIC_INFO|FLEX: enabled"
else
  BASIC_INFO="$BASIC_INFO|FLEX: disabled"
fi

if test "x$tr_BISON" = xyes;  then
  m4_include([m4/configure.bison])
  BASIC_INFO="$BASIC_INFO|BISON: enabled"
else
  BASIC_INFO="$BASIC_INFO|BISON: disabled"
fi

if test "x$tr_RELATIVE" != xno;  then
  BASIC_INFO="$BASIC_INFO|RELATIVE PATHS: $tr_RELATIVE"
else
  BASIC_INFO="$BASIC_INFO|RELATIVE PATHS: disabled"
fi

FLAG_INFO="$FLAG_INFO|."

m4_include([m4/external.math])
m4_include([m4/external.ncurses]) # before readline
m4_include([m4/external.readline])
m4_include([m4/external.openssl])

if test "x$tr_ALL_IN_ONE" = xyes;  then
  m4_include([m4/all-in-one.boost])
  m4_include([m4/all-in-one.libev])
  m4_include([m4/all-in-one.v8])
  BASIC_INFO="$BASIC_INFO|ALL-IN-ONE: enabled"
else
  m4_include([m4/external.boost])
  m4_include([m4/external.libev])
  m4_include([m4/external.v8])
  BASIC_INFO="$BASIC_INFO|ALL-IN-ONE: disabled"
fi

CPPFLAGS="$CPPFLAGS -DGEO_TRIAGENS=1"

dnl ============================================================================
dnl OUTPUT                                                        GENERATE FILES
dnl ============================================================================

BUILD_H="\$(top_srcdir)/build.h"
AC_SUBST(BUILD_H)

AC_CONFIG_FILES([Makefile Doxygen/avocado.doxy])
AC_CONFIG_HEADER([config/config.h BasicsC/local-configuration.h])
AC_OUTPUT

dnl ============================================================================
dnl INFORMATION                                   information about used options
dnl ============================================================================

m4_include([m4/configure.information])
