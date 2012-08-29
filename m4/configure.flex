dnl -*- mode: Autoconf; -*-

dnl ----------------------------------------------------------------------------
dnl --SECTION--                                                             FLEX
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(flex,
  AS_HELP_STRING([--enable-flex], [enable FLEX support (default: no)]),
  [tr_FLEX="${enableval:-yes}"],
  [tr_FLEX="no"]
)

AM_CONDITIONAL(ENABLE_FLEX, test "x$tr_FLEX" = xyes)

dnl ----------------------------------------------------------------------------
dnl check for flex
dnl ----------------------------------------------------------------------------

if test "x$tr_FLEX" = xyes;  then
  AC_MSG_NOTICE([................................................................................])
  AC_MSG_NOTICE([CHECKING FLEX])
  AC_MSG_NOTICE([................................................................................])

  AC_PROG_LEX

  tr_flex_usable=no

  if test "x$LEX" != x -a "x$LEX" != "x:";  then
    AC_MSG_CHECKING([if flex supports required features])

    [tr_flex_version=`$LEX --version | sed -e 's:^[^0-9]*::' | awk -F. '{print $1 * 10000 + $2 * 100 + $3}'`]

    if test "$tr_flex_version" -ge 20535;  then
      tr_flex_usable=yes
    fi

    AC_MSG_RESULT($tr_flex_usable)
  fi

  if test "x$tr_flex_usable" != "xyes"; then
    AC_MSG_ERROR([Please install flex version 2.5.35 or higher from http://ftp.gnu.org/gnu/flex/])
  fi

  AC_MSG_CHECKING([FLEX version])
  TRI_FLEX_VERSION=`$LEX --version`
  AC_MSG_RESULT([$TRI_FLEX_VERSION])

  AM_CONDITIONAL(ENABLE_FLEX, test "x$tr_flex_usable" = xyes)

  BASIC_INFO="$BASIC_INFO|FLEX: ${TRI_FLEX_VERSION}"
else
  BASIC_INFO="$BASIC_INFO|FLEX: disabled"
fi

dnl ----------------------------------------------------------------------------
dnl --SECTION--                                                      END-OF-FILE
dnl ----------------------------------------------------------------------------

dnl Local Variables:
dnl mode: outline-minor
dnl outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
dnl End:
