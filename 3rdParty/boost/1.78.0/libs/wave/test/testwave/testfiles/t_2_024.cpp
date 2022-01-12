/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

//O --c++17
//O -Werror

// Test __has_include() with system paths

// point system path to this directory
//O -S.

#ifdef __has_include
#if __has_include(<t_2_024.cpp>)
#define FOUND_SELF_VIA_SYSTEM_PATH
#else
#warning could not find this file via system path
#endif
#else
#warning has_include is not defined
#endif

//H 10: t_2_024.cpp(18): #ifdef
//H 11: t_2_024.cpp(18): #ifdef __has_include: 1
//H 10: t_2_024.cpp(19): #if
//H 11: t_2_024.cpp(19): #if __has_include(<t_2_024.cpp>): 1
//H 10: t_2_024.cpp(20): #define
//H 08: t_2_024.cpp(20): FOUND_SELF_VIA_SYSTEM_PATH=
//H 10: t_2_024.cpp(21): #else
//H 10: t_2_024.cpp(24): #else

#ifdef __has_include
#if 0
#elif __has_include(<t_2_024.cpp>)
#define FOUND_SELF_VIA_SYSTEM_PATH_ELIF
#else
#warning could not find this file via system path - elif
#endif
#endif

//H 10: t_2_024.cpp(37): #ifdef
//H 11: t_2_024.cpp(37): #ifdef __has_include: 1
//H 10: t_2_024.cpp(38): #if
//H 11: t_2_024.cpp(38): #if 0: 0
//H 10: t_2_024.cpp(39): #elif
//H 11: t_2_024.cpp(39): #elif __has_include(<t_2_024.cpp>): 1
//H 10: t_2_024.cpp(40): #define
//H 08: t_2_024.cpp(40): FOUND_SELF_VIA_SYSTEM_PATH_ELIF=
//H 10: t_2_024.cpp(41): #else
//H 10: t_2_024.cpp(44): #endif

#ifdef __has_include
#if !__has_include(<some_include_file.h>)
#define NOTFOUND_AS_EXPECTED
#else
#warning found nonexistent file
#endif
#else
#warning has_include is not defined
#endif

//H 10: t_2_024.cpp(57): #ifdef
//H 11: t_2_024.cpp(57): #ifdef __has_include: 1
//H 10: t_2_024.cpp(58): #if
//H 11: t_2_024.cpp(58): #if !__has_include(<some_include_file.h>): 1
//H 10: t_2_024.cpp(59): #define
//H 08: t_2_024.cpp(59): NOTFOUND_AS_EXPECTED=
//H 10: t_2_024.cpp(60): #else
//H 10: t_2_024.cpp(63): #else

#ifdef __has_include
#if 0
#elif !__has_include(<some_include_file.h>)
#define NOTFOUND_AS_EXPECTED_ELIF
#else
#warning found nonexistent file - elif
#endif
#endif

//H 10: t_2_024.cpp(76): #ifdef
//H 11: t_2_024.cpp(76): #ifdef __has_include: 1
//H 10: t_2_024.cpp(77): #if
//H 11: t_2_024.cpp(77): #if 0: 0
//H 10: t_2_024.cpp(78): #elif
//H 11: t_2_024.cpp(78): #elif !__has_include(<some_include_file.h>): 1
//H 10: t_2_024.cpp(79): #define
//H 08: t_2_024.cpp(79): NOTFOUND_AS_EXPECTED_ELIF=
//H 10: t_2_024.cpp(80): #else
//H 10: t_2_024.cpp(83): #endif

