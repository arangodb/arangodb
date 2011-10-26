dnl @synopsis AX_CXX_CHECK_LIB(libname, include, functioname, action-if, action-if-not, libs)

AC_DEFUN([AX_CXX_CHECK_LIB],
  [m4_ifval([$4], , [AH_CHECK_LIB([$1])])dnl
   AS_LITERAL_IF([$1],
                 [AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1_$3])],
                 [AS_VAR_PUSHDEF([ac_Lib], [ac_cv_lib_$1''_$3])])dnl
   AC_CACHE_CHECK([for $3 in -l$1], ac_Lib,
                  [ac_check_lib_save_LIBS=$LIBS
                   LIBS="-l$1 $6 $LIBS"
                   AC_LINK_IFELSE([AC_LANG_PROGRAM([
$2
                                                   ],
                                                   [
$3
                                                   ])],
                                  [AS_VAR_SET(ac_Lib, yes)],
                                  [AS_VAR_SET(ac_Lib, no)])
                   LIBS=$ac_check_lib_save_LIBS])
   AS_IF([test AS_VAR_GET(ac_Lib) = yes], 
         [m4_default([$4], [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_LIB$1)) LIBS="-l$1 $LIBS"])],
         [$5])dnl
   AS_VAR_POPDEF([ac_Lib])dnl
])# AC_CHECK_LIB
