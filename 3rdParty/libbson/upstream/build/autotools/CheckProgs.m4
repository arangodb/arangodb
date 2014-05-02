AC_PATH_PROG(PERL, perl)
if test -z "$PERL"; then
    AC_MSG_ERROR([You need 'perl' to compile libbson])
fi

AC_PATH_PROG(MV, mv)
if test -z "$MV"; then
    AC_MSG_ERROR([You need 'mv' to compile libbson])
fi

AC_PATH_PROG(GREP, grep)
if test -z "$GREP"; then
    AC_MSG_ERROR([You need 'grep' to compile libbson])
fi

AC_PROG_INSTALL

AC_ARG_VAR([XMLTO], [Path to xmlto command])
AC_PATH_PROG([XMLTO], [xmlto])
AC_ARG_VAR([ASCIIDOC], [Path to asciidoc command])
AC_PATH_PROG([ASCIIDOC], [asciidoc])

BSON_CHECK_DOC_BUILD
