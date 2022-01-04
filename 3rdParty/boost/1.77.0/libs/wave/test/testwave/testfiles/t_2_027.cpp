/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Test error reporting on undefinition of '__has_include'

//O --c++17
//O -Werror

#undef __has_include
//E t_2_027.cpp(15): warning: #undef may not be used on this predefined name: __has_include
