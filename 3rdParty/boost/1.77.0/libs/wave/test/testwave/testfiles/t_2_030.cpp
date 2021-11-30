/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Check for malformed computed includes

//O --c++17
//O -Werror

// macro that does *not* evaluate to a quoted or bracketed string
#define FOO BAR
#if __has_include(FOO)
#warning for some reason has_include likes this non-file-looking macro
#endif

//E t_2_030.cpp(17): error: ill formed __has_include expression: BAR
