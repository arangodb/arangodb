dnl ============================================================================
dnl ArangoDB                                                   (c) ArangoDB GmbH
dnl ============================================================================

dnl ============================================================================
dnl --SECTION--                                  ArangoDB GmbH Build Environment
dnl ============================================================================

AC_INIT([ArangoDB GmbH], [2.8.0-devel], [info@arangodb.com], [arangodb], [https://www.arangodb.com])

CURRENT_DIR=`pwd`

AC_MSG_NOTICE([configure started in '$CURRENT_DIR])
AC_MSG_NOTICE([with CC='$CC'])
AC_MSG_NOTICE([with CXX='$CXX'])
AC_MSG_NOTICE([with CPPFLAGS='$CPPFLAGS'])
AC_MSG_NOTICE([with CFLAGS='$CFLAGS'])
AC_MSG_NOTICE([with CXXFLAGS='$CXXFLAGS'])
AC_MSG_NOTICE([with LDFLAGS='$LDFLAGS'])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES(yes)])

m4_include([m4/configure.basics])

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
dnl --SECTION--                                         CHECKING THE ENVIRONMENT
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl python
dnl ----------------------------------------------------------------------------

if python -c '
import sys
if sys.version_info.major != 2:
    exit(0)
else:
    exit(1)
'; then
   echo "Compiling V8 requires a python version 2; please adjust your " `which python`
   exit 1
fi

dnl ----------------------------------------------------------------------------
dnl check for special OS like MacOS X or FreeBSD
dnl ----------------------------------------------------------------------------

tr_DARWIN="no"
tr_FREEBSD="no"
tr_SOLARIS="no"
tr_ARM="no"

case $target in
  *-apple-darwin*)
    tr_DARWIN="yes"
    ;;

  *-*-freebsd*)
    tr_FREEBSD="yes"
    ;;

  *-*-solaris*)
    tr_SOLARIS="yes"
    CC="gcc -std=gnu99"
    CPPFLAGS=" -I/opt/local/include -L/opt/local/lib"
    CXXFLAGS="-static-libstdc++ -D_GLIBCXX_USE_C99"
    LDFLAGS="-Wl,-rpath,/opt/local/lib,-lnsl,-lsocket -L/opt/local/lib"
    LIBEV_FLAGS="${LIBEV_FLAGS} -D _REENTRANT=1 -D HAVE_PORT_H=1 -D EV_USE_PORT=1 -D EV_USE_INOTIFY=0 -D EV_USE_EVENTFD=0"
    ;;

  armv7l-*-linux-gnueabihf)
    CXXFLAGS="${CXXFLAGS} -O0"
    tr_ARM="yes"
    tr_ARM7="yes"
    ;;

  armv6l-*-linux-gnueabihf)
    dnl ----------------------------------------------------------------------------
    dnl special flags for Arm V6 (pi)
    dnl ----------------------------------------------------------------------------
    CPPFLAGS="${CPPFLAGS} -O0 -DUSE_EABI_HARDFLOAT -march=armv6 -mfloat-abi=hard"
    CXXFLAGS="${CXXFLAGS} -O0"
    tr_ARM="yes"
    tr_ARM6="yes"
    ;;
esac

AM_CONDITIONAL(ENABLE_DARWIN, test "x$tr_DARWIN" = xyes)
AM_CONDITIONAL(ENABLE_FREEBSD, test "x$tr_FREEBSD" = xyes)
AM_CONDITIONAL(ENABLE_SOLARIS, test "x$tr_SOLARIS" = xyes)
AM_CONDITIONAL(ENABLE_ARM, test "x$tr_ARM" = xyes)
AM_CONDITIONAL(ENABLE_ARMV6, test "x$tr_ARM6" = xyes)
AM_CONDITIONAL(ENABLE_ARMV7, test "x$tr_ARM7" = xyes)

dnl ----------------------------------------------------------------------------
dnl checks for compiler and basic settings
dnl ----------------------------------------------------------------------------

AC_MSG_NOTICE([................................................................................])
AC_MSG_NOTICE([CHECKING C/C++ COMPILER AND LINKER])
AC_MSG_NOTICE([................................................................................])

case $target in
  *-*-cygwin)
    LDFLAGS="$LDFLAGS -Wl,--enable-auto-import"
    enable_static="yes"
    enable_shared="no"

    list_cc="[gcc-5 gcc-4 gcc-3 gcc cc]"
    list_cxx="[g++-5 g++-4 g++-3 g++ cxx]"

    ;;

  *) 
    list_cc="[${CC} gcc clang cc]"
    list_cxx="[${CXX} g++ clang++ cxx]"

    ;;
esac

AC_PROG_CXXCPP
AC_PROG_CC($list_cc)
AC_PROG_CXX($list_cxx)
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AX_CXX_COMPILE_STDCXX_11(noext, mandatory)
AX_CXX_CHECK_UNORDERED_MAP_EMPLACE


dnl ----------------------------------------------------------------------------
dnl bits
dnl ----------------------------------------------------------------------------

tr_BITS=32

case $target_cpu in
  amd64*)
    tr_BITS=64
    ;;

  x86_64*)
    tr_BITS=64
    ;;
esac

TRI_BITS="$tr_BITS"
AC_SUBST(TRI_BITS)

CPPFLAGS="${CPPFLAGS} -DTRI_BITS=${TRI_BITS}"

dnl ----------------------------------------------------------------------------
dnl options C/C++ standard
dnl ----------------------------------------------------------------------------

if test x$GCC == xyes;  then
  # try to select the correct C standard: gnu90 is nice, because it warns about constructs
  # the Visual Studio Compiler does not like

  GCCSTD=""

  TRI_TRY_CC_OPTION([-std=gnu90],
	            [GCCSTD="-std=gnu90"],
	            [TRI_TRY_CC_OPTION([-std=gnu89],
                                       [GCCSTD="-std=gnu89"])])

  GXXSTD=""

  TRI_TRY_CXX_OPTION([-std=c++11],
	             [GXXSTD="-std=c++11"])

  CFLAGS="${GCCSTD} ${CFLAGS}"
  CXXFLAGS="${GXXSTD} ${CXXFLAGS}"
fi

if test x$tr_DARWIN == xyes;  then
  TRI_TRY_CXX_OPTION([-stdlib=libc++],
                     [tr_CXX_HAS_STDLIB="yes"],
                     [tr_CXX_HAS_STDLIB="no"])
fi

AM_CONDITIONAL(CXX_HAS_STDLIB, test "x$tr_CXX_HAS_STDLIB" = "xyes")

dnl ----------------------------------------------------------------------------
dnl error & warnings && C/C++ standard
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(error-on-warning,
  AS_HELP_STRING([--enable-error-on-warning], [treat warnings as errors (default: no)]),
  [tr_WERROR="${enableval:-yes}"],
  [tr_WERROR=no]
)

CFLAGS="$CFLAGS $RPM_OPT_FLAGS"
CXXFLAGS="$CXXFLAGS $RPM_OPT_FLAGS"

if test x$GCC == xyes;  then
  WALL="-Wall -Winit-self -Wno-long-long -Wno-variadic-macros"

  if test x$tr_WERROR == xyes;  then
    WALL="${WALL} -Werror"
  fi

  if test x$tr_DARWIN == xyes;  then
    WALL="${WALL} -Wno-deprecated-declarations"
  fi

  WALLC="${WALL} -Wshadow -Wstrict-prototypes -Wdeclaration-after-statement"
  WALLCXX="${WALL} -Woverloaded-virtual"

  TRI_TRY_CXX_OPTION([-Wstrict-null-sentinel],
                     [WALLCXX="${WALLCXX} -Wstrict-null-sentinel"])
  
  # suppress warnings about unused compiler options. 
  # this is useful for clang as it complains about -std=... in linking 
  TRI_TRY_CXX_OPTION([-Qunused-arguments],
                     [WALLCXX="${WALLCXX} -Qunused-arguments"])


  CFLAGS="${CFLAGS} ${WALLC}"
  CXXFLAGS="${CXXFLAGS} ${WALLCXX}"
fi

dnl ----------------------------------------------------------------------------
dnl option for gnu style include
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(isystem,
  AS_HELP_STRING([--enable-isystem], [enables the use of -isystem instead of -I for system headers (default: no)]),
  [tr_ISYSTEM="${enableval:-yes}"],
  [tr_ISYSTEM=no]
)

if test x$GCC == xyes;  then
  INCPREFIX="-isystem " # note that the trailing blank is important
else
  INCPREFIX="-I"
fi

if test "x$tr_ISYSTEM" = "xno";  then
  INCPREFIX="-I"
fi

dnl ----------------------------------------------------------------------------
dnl use the gold linker
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(gold,
  AS_HELP_STRING([--enable-gold], [use new gold linker (default: no)]),
  [tr_GOLD="${enableval:-yes}"],
  [tr_GOLD=no]
)

AM_CONDITIONAL(ENABLE_GOLD, test "x$tr_GOLD" = xyes)

if test "x$tr_GOLD" = xyes;  then
  CFLAGS="$CFLAGS -fuse-ld=gold"
  CXXFLAGS="$CXXFLAGS -fuse-ld=gold"
fi

dnl ============================================================================
dnl --SECTION--                                                      SUB SYSTEMS
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl etcd
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(all-in-one-etcd,
  AS_HELP_STRING([--enable-all-in-one-etcd], [use supplied etcd from 3rdParty directory (default: yes)]),
  [tr_ALL_IN_ONE_ETCD="${enableval:-yes}"],
  [tr_ALL_IN_ONE_ETCD=yes]
)

AM_CONDITIONAL(ENABLE_ALL_IN_ONE_ETCD, test "x$tr_ALL_IN_ONE_ETCD" = xyes)

dnl ----------------------------------------------------------------------------
dnl go
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(internal-go,
  AS_HELP_STRING([--enable-internal-go], [use go binaries from 3rdParty directory (default: no)]),
  [tr_INTERNAL_GO="${enableval:-yes}"],
  [tr_INTERNAL_GO=no]
  GOPROG=x$tr_INTERNAL_GO
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
  GOPROG=$GO
else
  GOPROG=go
fi

AM_CONDITIONAL(GO_AVAILABLE, test "x$GOPROG" != x)

dnl ----------------------------------------------------------------------------
dnl use relative path
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

dnl ----------------------------------------------------------------------------
dnl rt
dnl ----------------------------------------------------------------------------

m4_include([m4/clock_gettime])
AC_CLOCK()

dnl ----------------------------------------------------------------------------
dnl splice
dnl ----------------------------------------------------------------------------

AC_MSG_CHECKING([whether your system knows about splice()]) 
AC_TRY_COMPILE([
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
],
[
  ssize_t sent, pipesize;
  int fd, SplicePipe[2];
  pipesize = splice(fd, NULL, 
                    SplicePipe[1], NULL, 
                    1, 
                    SPLICE_F_MORE | SPLICE_F_MOVE|SPLICE_F_NONBLOCK);
],
[
  ok_splice=yes
  AC_DEFINE_UNQUOTED(TRI_LINUX_SPLICE, 1, [true if we have the linux splice api])
  AC_MSG_RESULT([yes])
  AC_SUBST(TRI_LINUX_SPLICE)
],
[ 
  ok_splice=no
  AC_MSG_RESULT([no])
]
)

dnl ----------------------------------------------------------------------------
dnl load sub systems
dnl ----------------------------------------------------------------------------

m4_include([m4/configure.largefile])
m4_include([m4/configure.threads])
m4_include([m4/configure.documentation])
m4_include([m4/configure.coverage])
m4_include([m4/configure.maintainer])

AC_CHECK_FUNCS([futimes])

dnl ============================================================================
dnl --SECTION--                                               EXTERNAL LIBRARIES
dnl ============================================================================

dnl ----------------------------------------------------------------------------
dnl boost
dnl ----------------------------------------------------------------------------

BOOST_VERSION="1.58.0"
BOOST_CPPFLAGS="-I${srcdir}/3rdParty/boost/${BOOST_VERSION}"

AC_ARG_ENABLE(boost-tests,
  AS_HELP_STRING([--enable-boost-tests], [generate the unittests based on boost (default: $USE_MAINTAINER_MODE)]),
  [tr_BOOST_TESTS="${enableval:-yes}"],
  [tr_BOOST_TESTS="$USE_MAINTAINER_MODE"]
)

AM_CONDITIONAL(ENABLE_BOOST_TESTS, test "x$tr_BOOST_TESTS" = xyes)

AC_SUBST(BOOST_VERSION)
AC_SUBST(BOOST_CPPFLAGS)

LIB_INFO="$LIB_INFO|BOOST: 3rdParty version"
LIB_INFO="$LIB_INFO|BOOST VERSION: ${BOOST_VERSION}"
LIB_INFO="$LIB_INFO|BOOST_CPPFLAGS: ${BOOST_CPPFLAGS}"
LIB_INFO="$LIB_INFO|BOOST_TESTS: ${tr_BOOST_TESTS}"
LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl v8
dnl ----------------------------------------------------------------------------

V8_VERSION="4.3.61"
V8_DIR=V8-${V8_VERSION}

V8_CPPFLAGS="-I${srcdir}/3rdParty/${V8_DIR}/include -DTRI_V8_VERSION='\"${V8_VERSION}\"'"
V8_LDFLAGS=""
V8_TARGET="native"

v8_lib_dir=""

AC_ARG_ENABLE(v8-debug,
  AS_HELP_STRING([--enable-v8-debug], [build debug version of V8 (default: no)]),
  [tr_V8_DEBUG="${enableval:-yes}"],
  [tr_V8_DEBUG=no]
)

AM_CONDITIONAL(ENABLE_V8_DEBUG, test "x$tr_V8_DEBUG" = xyes)

if test x$tr_ARM == xyes;  then
  if test x$tr_V8_DEBUG == xyes;  then
    V8_TARGET="arm.debug"
  else
    V8_TARGET="arm.release"
  fi

  v8_lib_dir="${srcdir}/3rdParty/${V8_DIR}/out/$V8_TARGET/obj.target/tools/gyp"

elif test x$tr_DARWIN == xyes;  then
  if test "x$tr_BITS" == x64;  then
    if test x$tr_V8_DEBUG == xyes;  then
      V8_TARGET="x64.debug"
    else
      V8_TARGET="x64.release"
    fi
  else
    if test x$tr_V8_DEBUG == xyes;  then
      V8_TARGET="ia32.debug"
    else
      V8_TARGET="ia32.release"
    fi
  fi

  v8_lib_dir="${srcdir}/3rdParty/${V8_DIR}/out/$V8_TARGET"

else
  if test "x$tr_BITS" == x64;  then
    if test x$tr_V8_DEBUG == xyes;  then
      V8_TARGET="x64.debug"
    else
      V8_TARGET="x64.release"
    fi
  else
    if test x$tr_V8_DEBUG == xyes;  then
      V8_TARGET="ia32.debug"
    else
      V8_TARGET="ia32.release"
    fi
  fi

  v8_lib_dir="${srcdir}/3rdParty/${V8_DIR}/out/$V8_TARGET/obj.target/tools/gyp"
fi

V8_LIBS="${v8_lib_dir}/libv8_base.a ${v8_lib_dir}/libv8_libbase.a ${v8_lib_dir}/libv8_libplatform.a ${v8_lib_dir}/libv8_nosnapshot.a"

if test "x$tr_FREEBSD" == xyes;  then
  V8_LIBS="$V8_LIBS -lexecinfo"
fi

AC_SUBST(V8_VERSION)
AC_SUBST(V8_DIR)
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
dnl icu (after v8)
dnl ----------------------------------------------------------------------------

ICU_VERSION="52.2"
ICU_CPPFLAGS="-D_REENTRANT -I${srcdir}/3rdParty/${V8_DIR}/third_party/icu/source/common -I${srcdir}/3rdParty/${V8_DIR}/third_party/icu/source/i18n -I${srcdir}/3rdParty/${V8_DIR}/third_party/icu/source/io"
ICU_LDFLAGS=""

if test "x$tr_DARWIN" = xyes; then
  icu_lib_dir="${srcdir}/3rdParty/${V8_DIR}/out/$V8_TARGET"
else
  icu_lib_dir="${srcdir}/3rdParty/${V8_DIR}/out/$V8_TARGET/obj.target/third_party/icu"
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
dnl libev
dnl ----------------------------------------------------------------------------

LIBEV_VERSION="4.11"

LIBEV_CPPFLAGS="-I${srcdir}/3rdParty/libev -DTRI_LIBEV_VERSION='\"${LIBEV_VERSION}\"'"
LIBEV_CPPFLAGS="${LIBEV_CPPFLAGS}"
LIBEV_LDFLAGS=""
LIBEV_LIBS="${srcdir}/3rdParty/libev/BUILD/.libs/libev.a"
LIBEV_FLAGS="${LIBEV_FLAGS}"

AC_SUBST(LIBEV_VERSION)
AC_SUBST(LIBEV_CPPFLAGS)
AC_SUBST(LIBEV_LDFLAGS)
AC_SUBST(LIBEV_LIBS)
AC_SUBST(LIBEV_FLAGS)

LIB_INFO="$LIB_INFO|LIBEV: 3rdParty version"
LIB_INFO="$LIB_INFO|LIBEV VERSION: ${LIBEV_VERSION}"

LIB_INFO="$LIB_INFO|LIBEV_CPPFLAGS: ${LIBEV_CPPFLAGS}"
LIB_INFO="$LIB_INFO|LIBEV_LDFLAGS:  ${LIBEV_LDFLAGS}"
LIB_INFO="$LIB_INFO|LIBEV_LIBS:     ${LIBEV_LIBS}"
LIB_INFO="$LIB_INFO|LIBEV_FLAGS:    ${LIBEV_FLAGS}"

LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl math
dnl ----------------------------------------------------------------------------

m4_include([m4/external.math])

dnl ----------------------------------------------------------------------------
dnl repl console
dnl ----------------------------------------------------------------------------

AC_MSG_NOTICE([--------------------------------------------------------------------------------])
AC_MSG_NOTICE([CHECKING FOR CONSOLE SUPPORT])
AC_MSG_NOTICE([--------------------------------------------------------------------------------])

CONSOLE_CPPFLAGS=""

AC_ARG_ENABLE(console,
  AS_HELP_STRING([--enable-console], [enable console support (default: linenoise)]),
  tr_CONSOLE="$enableval",
  tr_CONSOLE="linenoise"
)

if test "$tr_CONSOLE" = "dummy"; then
  LIB_INFO="$LIB_INFO|CONSOLE: dummy"
elif test "$tr_CONSOLE" = "linenoise"; then
  LIB_INFO="$LIB_INFO|CONSOLE: linenoise"
  CONSOLE_CPPFLAGS="-I${srcdir}/3rdParty/linenoise-ng/include -I${srcdir}/3rdParty/linenoise-ng/src -DTRI_HAVE_LINENOISE"
fi

LIB_INFO="$LIB_INFO|CONSOLE_CPPFLAGS: ${CONSOLE_CPPFLAGS}"
LIB_INFO="$LIB_INFO|."

AC_SUBST(CONSOLE_CPPFLAGS)

AM_CONDITIONAL(ENABLE_LINENOISE, test "x$tr_CONSOLE" = xlinenoise)

dnl ----------------------------------------------------------------------------
dnl tcmalloc
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(tcmalloc,
  AS_HELP_STRING([--enable-tcmalloc], [use system-installed tcmalloc library (default: no)]),
  [tr_TCMALLOC="${enableval:-yes}"],
  [tr_TCMALLOC=no]
)

if test "x$tr_TCMALLOC" = "xyes"; then
  AC_MSG_CHECKING([tcmalloc])

  AC_SEARCH_LIBS([tc_malloc], [tcmalloc tcmalloc_minimal tcmalloc_debug], [ ], [ AC_MSG_ERROR([unable to find the tc_malloc library function]) ])
fi

AM_CONDITIONAL(ENABLE_TCMALLOC, test "x$tr_TCMALLOC" = xyes)

if test "x$tr_TCMALLOC" = "xyes"; then
  TCMALLOC_LIBS="-ltcmalloc"
fi
AC_SUBST(TCMALLOC_LIBS)

if test "x$tr_TCMALLOC" = "xyes";  then
  AC_DEFINE_UNQUOTED(TRI_HAVE_TCMALLOC, 1, [true if tcmalloc is used])

  BASIC_INFO="$BASIC_INFO|TCMALLOC support: enabled ($TCMALLOC_LIBS)"
else
  BASIC_INFO="$BASIC_INFO|TCMALLOC support: disabled"
fi

dnl ----------------------------------------------------------------------------
dnl openssl
dnl ----------------------------------------------------------------------------

m4_include([m4/external.openssl])

dnl ----------------------------------------------------------------------------
dnl zlib
dnl ----------------------------------------------------------------------------

ZLIB_VERSION="1.2.7"

ZLIB_CPPFLAGS=""
ZLIB_LDFLAGS=""
ZLIB_LIBS="${srcdir}/3rdParty/zlib-${ZLIB_VERSION}/libz.a"

AC_SUBST(ZLIB_VERSION)
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

dnl ----------------------------------------------------------------------------
dnl information
dnl ----------------------------------------------------------------------------

if test "x$BASIC_INFO" != "x";  then
  AC_MSG_NOTICE([--------------------------------------------------------------------------------])

  echo $BASIC_INFO | tr "|" "\n" | sort | while read a; do
    if test "x$a" != "x";  then
      AC_MSG_NOTICE([$[]a])
    fi
  done
fi

if test "x$LIB_INFO" != "x";  then
  AC_MSG_NOTICE([--------------------------------------------------------------------------------])

  echo $LIB_INFO | tr "|" "\n" | while read a; do
    if test "x$a" != "x";  then
      AC_MSG_NOTICE([$[]a])
    fi
  done
fi

if test "x$TOOL_INFO" != "x";  then
  AC_MSG_NOTICE([--------------------------------------------------------------------------------])

  echo $TOOL_INFO | tr "|" "\n" | sort | while read a; do
    if test "x$a" != "x";  then
      AC_MSG_NOTICE([$[]a])
    fi
  done
fi

AC_MSG_NOTICE([--------------------------------------------------------------------------------])

AC_MSG_NOTICE([REPOSITORY VERSION: ${TRI_REPOSITORY_VERSION}])
AC_MSG_NOTICE([BITS: ${TRI_BITS}])
AC_MSG_NOTICE([CPPFLAGS: ${CPPFLAGS}])
AC_MSG_NOTICE([CFLAGS: ${CFLAGS}])
AC_MSG_NOTICE([CXXFLAGS: ${CXXFLAGS}])
AC_MSG_NOTICE([LDFLAGS: ${LDFLAGS}])
AC_MSG_NOTICE([LIBS: ${LIBS}])

CC_VERSION=`${CC} --version 2>&1`
AC_MSG_NOTICE([CC: ${CC}])
AC_MSG_NOTICE([CC VERSION: ${CC_VERSION}])
CXX_VERSION=`${CXX} --version 2>&1`
AC_MSG_NOTICE([CXX: ${CXX}])
AC_MSG_NOTICE([CXX VERSION: ${CXX_VERSION}])

echo $FLAG_INFO | tr "|" "\n" | while read a; do
  if test "x$a" != "x";  then
    AC_MSG_NOTICE([$[]a])
  fi
done

AC_MSG_NOTICE([--------------------------------------------------------------------------------])

dnl ============================================================================
dnl --SECTION--                                                      END-OF-FILE
dnl ============================================================================

dnl Local Variables:
dnl mode: outline-minor
dnl outline-regexp: "^\\(dnl [a-zA-Z]\\|dnl --SECTION--\\)"
dnl End:
