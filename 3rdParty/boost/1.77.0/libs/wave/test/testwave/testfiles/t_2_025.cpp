/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++17
//O -Werror

// Test __has_include() with computed includes

// point system path to this directory
//O -S.

// does this file exist?
#if __has_include(__FILE__)
#define GOTTHISFILE
#else
#warning has_include could not find with file predefined macro
#endif

//H 10: t_2_025.cpp(19): #if
//H 01: <built-in>(1): __FILE__
//H 02: "$F"
//H 03: "$F"
//H 11: t_2_025.cpp(19): #if __has_include(__FILE__): 1
//H 10: t_2_025.cpp(20): #define
//H 08: t_2_025.cpp(20): GOTTHISFILE=
//H 10: t_2_025.cpp(21): #else

// try a computation producing a quoted file
#define FOO(X) "t_2_025.cpp"
#define BAR 0
#if __has_include(FOO(BAR))
#define FOUND_DQUOTE
#else
#warning has_include could not find this file using quotes
#endif

//H 10: t_2_025.cpp(35): #define
//H 08: t_2_025.cpp(35): FOO(X)="t_2_025.cpp"
//H 10: t_2_025.cpp(36): #define
//H 08: t_2_025.cpp(36): BAR=0
//H 10: t_2_025.cpp(37): #if
//H 00: t_2_025.cpp(37): FOO(BAR), [t_2_025.cpp(35): FOO(X)="t_2_025.cpp"]
//H 02: "t_2_025.cpp"
//H 03: "t_2_025.cpp"
//H 11: t_2_025.cpp(37): #if __has_include(FOO(BAR)): 1
//H 10: t_2_025.cpp(38): #define
//H 08: t_2_025.cpp(38): FOUND_DQUOTE=
//H 10: t_2_025.cpp(39): #else

// try a computation producing a bracketed name
#define EMPTY
#define ASSEMBLE(ARG) <t_2_025.cpp>
#if __has_include(ASSEMBLE(EMPTY))
#define FOUND_ABRACKET
#else
#warning has_include could not find this file using angle brackets
#endif

//H 10: t_2_025.cpp(57): #define
//H 08: t_2_025.cpp(57): EMPTY=
//H 10: t_2_025.cpp(58): #define
//H 08: t_2_025.cpp(58): ASSEMBLE(ARG)=<t_2_025.cpp>
//H 10: t_2_025.cpp(59): #if
//H 00: t_2_025.cpp(59): ASSEMBLE(EMPTY), [t_2_025.cpp(58): ASSEMBLE(ARG)=<t_2_025.cpp>]
//H 02: <t_2_025.cpp>
//H 03: <t_2_025.cpp>
//H 11: t_2_025.cpp(59): #if __has_include(ASSEMBLE(EMPTY)): 1
//H 10: t_2_025.cpp(60): #define
//H 08: t_2_025.cpp(60): FOUND_ABRACKET=
//H 10: t_2_025.cpp(61): #else
