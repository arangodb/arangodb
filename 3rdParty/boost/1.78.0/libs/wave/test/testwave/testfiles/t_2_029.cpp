/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Test that __has_include is an available macro name before C++17

//O --c++11
//O -Werror

#ifdef __has_include
#warning has_include is available in C++11 mode
#endif

#define __has_include(X) something

//H 10: t_2_029.cpp(15): #ifdef
//H 11: t_2_029.cpp(15): #ifdef __has_include: 0
//H 10: t_2_029.cpp(19): #define
//H 08: t_2_029.cpp(19): __has_include(X)=something

