dnl -*- mode: Autoconf; -*-

dnl ----------------------------------------------------------------------------
dnl --SECTION--                                                            BISON
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(bison,
  AS_HELP_STRING([--enable-bison], [enable BISON support (default: no)]),
  [tr_BISON="${enableval:-yes}"],
  [tr_BISON=no]
)

AM_CONDITIONAL(ENABLE_BISON, test "x$tr_BISON" = xyes)

dnl ----------------------------------------------------------------------------
dnl check for bison
dnl ----------------------------------------------------------------------------

if test "x$tr_BISON" = xyes;  then
  AC_MSG_NOTICE([................................................................................])
  AC_MSG_NOTICE([CHECKING BISON])
  AC_MSG_NOTICE([................................................................................])

  AC_CHECK_PROG(BISON, bison, bison)

  tr_bison_usable=no

  if test "x$BISON" != x -a "x$BISON" != "x:"; then
    AC_MSG_CHECKING([if bison supports required features])

    [tr_bison_version=`$BISON --version | head -1 | sed -e 's:^[^0-9]*::' | awk -F. '{print $1 * 10000 + $2 * 100 + $3}'`] 

    if test "$tr_bison_version" -ge 20401; then
      tr_bison_usable=yes
    fi

    AC_MSG_RESULT($tr_bison_usable)
  fi

  if test "x$tr_bison_usable" != "xyes"; then
    AC_MSG_ERROR([Please install bison version 2.4.1 or higher from http://ftp.gnu.org/gnu/bison/])
  fi

  AC_MSG_CHECKING([BISON version])
  TRI_BISON_VERSION=`$BISON --version | head -1`
  AC_MSG_RESULT([$TRI_BISON_VERSION])

  AM_CONDITIONAL(ENABLE_BISON, test "x$tr_bison_usable" = xyes)

  BASIC_INFO="$BASIC_INFO|BISON: ${TRI_BISON_VERSION}"
else
  BASIC_INFO="$BASIC_INFO|BISON: disabled"
fi

dnl ----------------------------------------------------------------------------
dnl --SECTION--                                                      END-OF-FILE
dnl ----------------------------------------------------------------------------

dnl Local Variables:
dnl mode: outline-minor
dnl outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
dnl End:
