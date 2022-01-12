/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++20
//O -Werror

// tests for changes to variadics handling
// specifically, that arg list after named args can be empty

#define ok_to_be_empty(BASE, ...) __VA_ARGS__##BASE##__VA_ARGS__

//R #line 20 "t_8_002.cpp"
//R notemptyfillernotempty
ok_to_be_empty(filler, notempty)

//R #line 24 "t_8_002.cpp"
//R filler
ok_to_be_empty(filler)
