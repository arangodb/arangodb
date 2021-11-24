/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++20
//O -Werror

// test that __VA_OPT__ cannot be used in a macro definition without parens

// gcc detects this at definition time
//E t_8_009.cpp(17): error: __VA_OPT__ must be followed by a left paren in a C++20 variadic macro: FOO
#define FOO(...) BAR __VA_OPT__ BAR
