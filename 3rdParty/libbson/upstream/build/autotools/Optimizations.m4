OPTIMIZE_CFLAGS=""
OPTIMIZE_LDFLAGS=""

# Enable -Bsymbolic
AS_IF([test "$enable_optimizations" != "no"], [
    check_link_flag([-Wl,-Bsymbolic], [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -Wl,-Bsymbolic"])
])

# Enable Link-Time-Optimization
AS_IF([test "$enable_lto" = "yes"],
      [AS_IF([test "$c_compiler" = "gcc"],
          [check_cc_cxx_flag([-flto], [OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -flto"])
           check_link_flag([-flto], [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -flto"])],
          [AC_MSG_WARN([LTO is not yet available on your compiler.])])])

AC_SUBST(OPTIMIZE_CFLAGS)
AC_SUBST(OPTIMIZE_LDFLAGS)

# Add '-g' flag to gcc to build with debug symbols.
if test "$enable_debug_symbols" = "min"; then
    CFLAGS="$CFLAGS -g1"
elif test "$enable_debug_symbols" != "no"; then
    CFLAGS="$CFLAGS -g"
fi

# Add the appropriate 'O' level for optimized builds.
if test "$enable_optimizations" = "yes"; then
    CFLAGS="$CFLAGS -O2"

    if test "$c_compiler" = "gcc"; then
        CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
    fi
else
    CFLAGS="$CFLAGS -O0"
fi
