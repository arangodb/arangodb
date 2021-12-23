/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++17
//O -Werror

// Test __has_include() with some interesting cases

#if __has_include(/* comment */ __FILE__ /* comment */)
#define GOTTHISFILE_WITHCOMMENTS
#else
#warning cannot find using file macro when comments are present
#endif

//H 10: t_2_026.cpp(15): #if
//H 01: <built-in>(1): __FILE__
//H 02: "$F"
//H 03: "$F"
//H 11: t_2_026.cpp(15): #if __has_include(/* comment */ __FILE__ /* comment */): 1
//H 10: t_2_026.cpp(16): #define
//H 08: t_2_026.cpp(16): GOTTHISFILE_WITHCOMMENTS=
//H 10: t_2_026.cpp(17): #else


