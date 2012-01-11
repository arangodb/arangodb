dnl ============================================================================
dnl AvocadoDB                                                  (c) triAGENS GmbH
dnl ============================================================================

dnl ============================================================================
dnl PREAMBLE                                     triAGENS GmbH Build Environment
dnl ============================================================================

AC_INIT([triAGENS AvocadoDB], [0.0.6], [info@triagens.de], [avocado], [http://www.worldofvoc.com])

dnl ----------------------------------------------------------------------------
dnl auxillary directory for install-sh and missing
dnl ----------------------------------------------------------------------------

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

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
m4_include([m4/configure.flex-bison])
m4_include([m4/configure.dot])

FLAG_INFO="$FLAG_INFO|."

m4_include([m4/external.boost])
m4_include([m4/external.libev])
m4_include([m4/external.math])
m4_include([m4/external.ncurses])
m4_include([m4/external.readline])
m4_include([m4/external.openssl])
m4_include([m4/external.v8])

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
