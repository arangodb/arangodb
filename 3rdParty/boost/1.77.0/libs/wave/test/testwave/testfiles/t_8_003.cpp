/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++11
//O -Werror

// test that arg list after named args CANNOT be empty (in c++11 mode)

#define ok_to_be_empty(BASE, ...) __VA_ARGS__##BASE##__VA_ARGS__

//R #line 19 "t_8_003.cpp"
//R notemptyfillernotempty
ok_to_be_empty(filler, notempty)

//E t_8_003.cpp(22): warning: too few macro arguments: ok_to_be_empty
ok_to_be_empty(filler)
