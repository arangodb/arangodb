AC_PREREQ(2.60)

m4_include([build/autotools/Versions.m4])

AC_INIT([libbson],[bson_version],[https://github.com/mongodb/libbson/issues])
AC_CONFIG_MACRO_DIR([build/autotools/m4])
AC_CONFIG_AUX_DIR([build/autotools])
AC_SUBST(ACLOCAL_AMFLAGS, "-I build/autotools")

BSON_MAJOR_VERSION=bson_major_version
BSON_MINOR_VERSION=bson_minor_version
BSON_MICRO_VERSION=bson_micro_version
BSON_API_VERSION=1.0
BSON_VERSION=bson_version
AC_SUBST(BSON_MAJOR_VERSION)
AC_SUBST(BSON_MINOR_VERSION)
AC_SUBST(BSON_MICRO_VERSION)
AC_SUBST(BSON_API_VERSION)
AC_SUBST(BSON_VERSION)

m4_include([build/autotools/CheckCompiler.m4])
m4_include([build/autotools/CheckHeaders.m4])
m4_include([build/autotools/CheckProgs.m4])
m4_include([build/autotools/CheckHost.m4])
m4_include([build/autotools/CheckTarget.m4])
m4_include([build/autotools/Endian.m4])

m4_include([build/autotools/SetupLibtool.m4])
LT_INIT
BSON_LT_CURRENT=lt_current
BSON_LT_REVISION=lt_revision
BSON_LT_AGE=lt_age
BSON_LT_VERSION="$BSON_LT_CURRENT:$BSON_LT_REVISION:$BSON_LT_AGE"
BSON_LT_LDFLAGS="-version-info $BSON_LT_VERSION"

m4_include([build/autotools/ReadCommandLineArguments.m4])
m4_include([build/autotools/FindDependencies.m4])
m4_include([build/autotools/AutoHarden.m4])
m4_include([build/autotools/MaintainerFlags.m4])
m4_include([build/autotools/Optimizations.m4])
m4_include([build/autotools/Python.m4])

# We would put AM_INIT_AUTOMAKE into SetupAutomake.m4, but seems to cause autoconf errors.
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])
AM_MAINTAINER_MODE([enable])
m4_include([build/autotools/SetupAutomake.m4])

AC_CONFIG_FILES([
    Makefile
    src/libbson-1.0.pc
    src/bson/bson-config.h
    src/bson/bson-version.h
])

m4_include([build/autotools/PrintBuildConfiguration.m4])
