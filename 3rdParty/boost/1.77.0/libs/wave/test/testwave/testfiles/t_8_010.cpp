/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++20
//O -Werror

// test that __VA_OPT__ cannot be used inside itself

// gcc detects this at definition time
//E t_8_010.cpp(17): error: __VA_OPT__() may not contain __VA_OPT__: FOO
#define FOO(...) BAR __VA_OPT__(__VA_OPT__(,) BAZ) BAR

