/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Test error reporting during redefinition of '__has_include'

//O --c++17
//O -Werror

#define __has_include(X) something
//E t_2_028.cpp(15): warning: this predefined name may not be redefined: __has_include
