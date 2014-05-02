dnl ##############################################################################
dnl # BSON_CHECK_DOC_BUILD                                                       #
dnl # Check whether to build documentation and install man-pages                 #
dnl ##############################################################################
AC_DEFUN([BSON_CHECK_DOC_BUILD], [{
    # Allow user to disable doc build
    AC_ARG_WITH([documentation], [AS_HELP_STRING([--without-documentation],
        [disable documentation build even if asciidoc and xmlto are present [default=no]])])

    if test "x$with_documentation" = "xno"; then
        bson_build_doc="no"
        bson_install_man="no"
    else
        # Determine whether or not documentation should be built and installed.
        bson_build_doc="yes"
        bson_install_man="yes"
        # Check for asciidoc and xmlto and don't build the docs if these are not installed.
        AC_CHECK_PROG(bson_have_asciidoc, asciidoc, yes, no)
        AC_CHECK_PROG(bson_have_xmlto, xmlto, yes, no)
        if test "x$bson_have_asciidoc" = "xno" -o "x$bson_have_xmlto" = "xno"; then
            bson_build_doc="no"
            # Tarballs built with 'make dist' ship with prebuilt documentation.
            if ! test -f doc/bson.7; then
                bson_install_man="no"
                AC_MSG_WARN([You are building an unreleased version of Mongoc and asciidoc or xmlto are not installed.])
                AC_MSG_WARN([Documentation will not be built and manual pages will not be installed.])
            fi
        fi

        # Do not install man pages if on mingw
        if test "x$bson_on_mingw32" = "xyes"; then
            bson_install_man="no"
        fi
    fi

    AC_MSG_CHECKING([whether to build documentation])
    AC_MSG_RESULT([$bson_build_doc])

    AC_MSG_CHECKING([whether to install manpages])
    AC_MSG_RESULT([$bson_install_man])

    AM_CONDITIONAL(BUILD_DOC, test "x$bson_build_doc" = "xyes")
    AM_CONDITIONAL(INSTALL_MAN, test "x$bson_install_man" = "xyes")
}])
