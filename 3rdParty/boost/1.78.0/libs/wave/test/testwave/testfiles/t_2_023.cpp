/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++17
//O -Werror

// Test __has_include() returning false

#ifdef __has_include
#if __has_include("some_include_file.h")
#warning found header, but should not have
#elif __has_include("some_include_file.h")
#warning found header, but should not have : elif
#else
#define NOTFOUND_AS_EXPECTED
#endif
#else
#warning has_include does not seem to be working
#endif

//H 10: t_2_023.cpp(15): #ifdef
//H 11: t_2_023.cpp(15): #ifdef __has_include: 1
//H 10: t_2_023.cpp(16): #if
//H 11: t_2_023.cpp(16): #if __has_include("some_include_file.h"): 0
//H 10: t_2_023.cpp(18): #elif
//H 11: t_2_023.cpp(18): #elif __has_include("some_include_file.h"): 0
//H 10: t_2_023.cpp(21): #define
//H 08: t_2_023.cpp(21): NOTFOUND_AS_EXPECTED=
//H 10: t_2_023.cpp(22): #endif
//H 10: t_2_023.cpp(23): #else

#ifndef __has_include
#warning has_include appears to be disabled
#else
#define HAS_INCLUDE_WORKS_IFNDEF
#if __has_include("t_2_023.cpp")
#define HAS_INCLUDE_WORKS_FILECHECK
#endif
#if !__has_include("t_2_023.cpp")
#warning has_include cannot find the file it is in
#elif __has_include("t_2_023.cpp")
#define HAS_INCLUDE_ELIF_FILECHECK
#else
#warning has_include cannot find the file it is in - elif
#endif
#endif

//H 10: t_2_023.cpp(38): #ifndef
//H 11: t_2_023.cpp(38): #ifndef __has_include: 1
//H 10: t_2_023.cpp(41): #define
//H 08: t_2_023.cpp(41): HAS_INCLUDE_WORKS_IFNDEF=
//H 10: t_2_023.cpp(42): #if
//H 11: t_2_023.cpp(42): #if __has_include("t_2_023.cpp"): 1
//H 10: t_2_023.cpp(43): #define
//H 08: t_2_023.cpp(43): HAS_INCLUDE_WORKS_FILECHECK=
//H 10: t_2_023.cpp(44): #endif
//H 10: t_2_023.cpp(45): #if
//H 11: t_2_023.cpp(45): #if !__has_include("t_2_023.cpp"): 0
//H 10: t_2_023.cpp(47): #elif
//H 11: t_2_023.cpp(47): #elif __has_include("t_2_023.cpp"): 1
//H 10: t_2_023.cpp(48): #define
//H 08: t_2_023.cpp(48): HAS_INCLUDE_ELIF_FILECHECK=
//H 10: t_2_023.cpp(49): #else
//H 10: t_2_023.cpp(52): #endif


