/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++11
//O -Werror

// test that __VA_OPT__ is ignored when not in c++20 mode

#define FOO(BAR, ...) BAR __VA_OPT__(,) __VA_ARGS__

//R #line 19 "t_8_011.cpp"
//R something __VA_OPT__(,) other
FOO(something, other)
