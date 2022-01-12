/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++20
//O -Werror

// test that __VA_OPT__ cannot be used as a object-like macro

//E t_8_005.cpp(16): error: __VA_OPT__ can only appear in the expansion of a C++20 variadic macro: __VA_OPT__
#define __VA_OPT__ something somethingelse
