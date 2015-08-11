dnl ============================================================================
dnl ArangoDB                                                  (c) triAGENS GmbH
dnl ============================================================================

dnl ============================================================================
dnl --SECTION--                                  triAGENS GmbH Build Environment
dnl ============================================================================

AC_INIT([triAGENS ArangoDB], [2.7.0-alpha2], [info@arangodb.com], [arangodb], [https://www.arangodb.com])

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
fi

AM_CONDITIONAL(GO_AVAILABLE, test "x$GOPROG" != x)

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

AC_CHECK_FUNCS([futimes])

dnl ============================================================================
dnl --SECTION--                                               EXTERNAL LIBRARIES
dnl ============================================================================

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
  BASIC_INFO="$BASIC_INFO|TCMALLOC support: enabled ($TCMALLOC_LIBS)"
else
  BASIC_INFO="$BASIC_INFO|TCMALLOC support: disabled"
fi

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

AC_MSG_NOTICE([--------------------------------------------------------------------------------])
AC_MSG_NOTICE([CHECKING FOR READLINE])
AC_MSG_NOTICE([--------------------------------------------------------------------------------])

AC_LANG(C)

AC_ARG_ENABLE(readline,
  AS_HELP_STRING([--enable-readline], [enable readline support (default: yes)]),
  tr_READLINE="$enableval",
  tr_READLINE="maybe"
)

if test "x$tr_DARWIN" = xyes; then
  READLINE=/usr/local/opt/readline
  READLINE_CPPFLAGS="-I$READLINE/include"
  READLINE_LDFLAGS="-L$READLINE/lib"
fi

AC_ARG_WITH(readline,
  AS_HELP_STRING([--with-readline=DIR], [where the readline library and includes are located]),
  [READLINE_CPPFLAGS="-I$withval/include"
   READLINE_LDFLAGS="-L$withval/lib"
   READLINE="$withval"]
)

AC_ARG_WITH(readline-lib,
  AS_HELP_STRING([--with-readline-lib=DIR], [where the readline library is located]),
  [READLINE_LDFLAGS="-L$withval"]
)

dnl checks for the READLINE library

if test "x$READLINE_CPPFLAGS" != x;  then
  TR_INCLUDE([READLINE_CPPFLAGS])
fi

dnl save flags

SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $READLINE_CPPFLAGS"

SAVE_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $READLINE_LDFLAGS"

SAVE_LIBS="$LIBS"

dnl check for header and library

if test "x$tr_READLINE" = xyes -o "x$tr_READLINE" = xmaybe;  then
  ch_READLINE="$tr_READLINE"

  AC_CHECK_HEADERS(readline/readline.h, [tr_READLINE="yes"], [tr_READLINE="no"])

  if test "x$tr_READLINE" = xyes;  then
    AC_CHECK_LIB([readline], [readline], [READLINE_LIBS="-lreadline" tr_READLINE="yes"], [tr_READLINE="no"])
  fi

  AC_MSG_CHECKING([READLINE support])

  if test "x$tr_READLINE" != xyes;  then
    AC_MSG_RESULT([not found])

    if test "x$ch_READLINE" = xyes;  then
      AC_MSG_ERROR([Please install readline support])
    fi
  else
    AC_MSG_RESULT([readline])
  fi
else
  AC_MSG_CHECKING([READLINE support])
  AC_MSG_RESULT([disabled])
fi

dnl grep readline version number

if test "x$tr_READLINE" = xyes;  then

cat confdefs.h - <<_ACEOF >conftest.$ac_ext
/* end confdefs.h.  */
#include <readline/readline.h>

main () {
#if defined(RL_VERSION_MAJOR) && defined(RL_VERSION_MINOR)
  long sdnhg36ed = RL_VERSION_MAJOR RL_VERSION_MINOR ;
#else
  long sdnhg36ed = RL_READLINE_VERSION hex ;
#endif
}

_ACEOF

  AC_MSG_CHECKING([READLINE version])
  eval "$ac_cpp conftest.$ac_ext" | fgrep "long sdnhg36ed" | awk '{print $4 "." $5}' > conftest.output
  READLINE_VERSION=`cat conftest.output`

  if test -z "$READLINE_VERSION"; then
    AC_MSG_ERROR([Readline support is not working. Please re-install readline support])
  fi

  AC_MSG_RESULT([$READLINE_VERSION])
  rm -f conftest*

  if test "x$tr_DARWIN" = xyes; then
    case "$READLINE_VERSION" in
      6.*|5.*)
        ;;

      *)
        AC_MSG_ERROR([Please install readline 6 from brew. If you have a non-standard brew location, use "--with-readline".])
        ;;
    esac
  fi
fi

dnl restore flags and set readline flags

LIBS="$SAVE_LIBS"
LDFLAGS="$SAVE_LDFLAGS"
CPPFLAGS="$SAVE_CPPFLAGS"

if test "x$tr_READLINE" = xyes;  then
  CPPFLAGS="$CPPFLAGS -DHAVE_READLINE=1"
  READLINE_CPPFLAGS="${READLINE_CPPFLAGS} -DTRI_READLINE_VERSION='\"${READLINE_VERSION}\"'"
fi

AM_CONDITIONAL(ENABLE_READLINE, test "x$tr_READLINE" = xyes)

if test "x$tr_READLINE" = xyes;  then
  AC_DEFINE_UNQUOTED(TRI_HAVE_READLINE, 1, [true if readline is used])
fi

dnl add substitutions

AC_SUBST(READLINE_VERSION)
AC_SUBST(READLINE_CPPFLAGS)
AC_SUBST(READLINE_LDFLAGS)
AC_SUBST(READLINE_LIBS)

dnl informational output

if test "x$tr_READLINE" = xyes;  then
  LIB_INFO="$LIB_INFO|READLINE VERSION:  ${READLINE_VERSION}"

  LIB_INFO="$LIB_INFO|READLINE_CPPFLAGS: ${READLINE_CPPFLAGS}"
  LIB_INFO="$LIB_INFO|READLINE_LDLIBS:   ${READLINE_LDLIBS}"
  LIB_INFO="$LIB_INFO|READLINE_LIBS:     ${READLINE_LIBS}"

elif test "x$tr_READLINE" = xlinenoise;  then
  LIB_INFO="$LIB_INFO|LINENOISE VERSION:  ${READLINE_VERSION}"

  LIB_INFO="$LIB_INFO|LINENOISE_CPPFLAGS: ${READLINE_CPPFLAGS}"
  LIB_INFO="$LIB_INFO|LINENOISE_LDLIBS:   ${READLINE_LDLIBS}"
  LIB_INFO="$LIB_INFO|LINENOISE_LIBS:     ${READLINE_LIBS}"

else
  LIB_INFO="$LIB_INFO|READLINE VERSION: disabled"
fi

LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl OPENSSL
dnl ----------------------------------------------------------------------------

m4_include([m4/external.openssl])

dnl ----------------------------------------------------------------------------
dnl BOOST
dnl ----------------------------------------------------------------------------

BOOST_VERSION="1.58.0"
BOOST_CPPFLAGS="-I${srcdir}/3rdParty/boost/${BOOST_VERSION}"

AC_SUBST(BOOST_VERSION)
AC_SUBST(BOOST_CPPFLAGS)

LIB_INFO="$LIB_INFO|BOOST: 3rdParty version"
LIB_INFO="$LIB_INFO|BOOST VERSION: ${BOOST_VERSION}"

LIB_INFO="$LIB_INFO|BOOST_CPPFLAGS: ${BOOST_CPPFLAGS}"
LIB_INFO="$LIB_INFO|."

dnl ----------------------------------------------------------------------------
dnl V8
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
dnl ICU
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
dnl LIBEV
dnl ----------------------------------------------------------------------------

LIBEV_VERSION="4.11"

LIBEV_CPPFLAGS="-I${srcdir}/3rdParty/libev -DTRI_LIBEV_VERSION='\"${LIBEV_VERSION}\"'"
LIBEV_CPPFLAGS="${LIBEV_CPPFLAGS}"
LIBEV_LDFLAGS=""
LIBEV_LIBS="${srcdir}/3rdParty/libev/BUILD/.libs/libev.a"

AC_SUBST(LIBEV_VERSION)
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

AC_SUBST(ZLIB_VERSION)
AC_SUBST(ZLIB_CPPFLAGS)
AC_SUBST(ZLIB_LDFLAGS)
AC_SUBST(ZLIB_LIBS)

LIB_INFO="$LIB_INFO|ZLIB: 3rdParty version"
LIB_INFO="$LIB_INFO|ZLIB VERSION: ${ZLIB_VERSION}"

LIB_INFO="$LIB_INFO|ZLIB_CPPFLAGS: ${ZLIB_CPPFLAGS}"
LIB_INFO="$LIB_INFO|ZLIB_LDFLAGS:  ${ZLIB_LDFLAGS}"
LIB_INFO="$LIB_INFO|ZLIB_LIBS:     ${ZLIB_LIBS}"

dnl ----------------------------------------------------------------------------
dnl SPLICE
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
