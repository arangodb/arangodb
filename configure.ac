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
m4_include([m4/configure.static])
m4_include([m4/configure.32bit])
m4_include([m4/configure.largefile])
m4_include([m4/configure.threads])
m4_include([m4/configure.flex-bison])
m4_include([m4/configure.boost])
m4_include([m4/configure.libev])
m4_include([m4/configure.openssl])
m4_include([m4/configure.v8])

dnl ============================================================================
dnl OUTPUT                                                        GENERATE FILES
dnl ============================================================================

BUILD_H="\$(top_srcdir)/build.h"
AC_SUBST(BUILD_H)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl ============================================================================
dnl INFORMATION                                   information about used options
dnl ============================================================================

m4_include([m4/configure.information])
