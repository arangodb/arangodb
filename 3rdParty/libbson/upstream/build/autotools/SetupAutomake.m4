m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_PROG_CC_C_O

# OS Conditionals.
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])
AM_CONDITIONAL([OS_UNIX],[test "$os_win32" = "no"])
AM_CONDITIONAL([OS_LINUX],[test "$os_linux" = "yes"])
AM_CONDITIONAL([OS_GNU],[test "$os_gnu" = "yes"])
AM_CONDITIONAL([OS_DARWIN],[test "$os_darwin" = "yes"])
AM_CONDITIONAL([OS_FREEBSD],[test "$os_freebsd" = "yes"])

# Compiler Conditionals.
AM_CONDITIONAL([COMPILER_GCC],[test "$c_compiler" = "gcc" && test "$cxx_compiler" = "g++"])
AM_CONDITIONAL([COMPILER_CLANG],[test "$c_compiler" = "clang" && test "$cxx_compiler" = "clang++"])

# Feature Conditionals
AM_CONDITIONAL([ENABLE_DEBUG],[test "$enable_debug" = "yes"])

# Bindings Conditionals
AM_CONDITIONAL([ENABLE_PYTHON],[test "$enable_python" = "yes"])

# C99 Features
AM_CONDITIONAL([ENABLE_STDBOOL],[test "$enable_stdbool" = "yes"])

# Should we use pthreads
AM_CONDITIONAL([ENABLE_PTHREADS], test "$enable_pthreads" = "yes")
