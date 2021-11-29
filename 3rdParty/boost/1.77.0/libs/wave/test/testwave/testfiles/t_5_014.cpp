/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2012 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    The tests included in this file were initially taken from the mcpp V2.5
    preprocessor validation suite and were modified to fit into the Boost.Wave 
    unit test requirements.
    The original files of the mcpp preprocessor are distributed under the 
    license reproduced at the end of this file.
=============================================================================*/

// Tests integer preprocessing number token and type of #if expression.

//O --long_long

// 12.1:
//R #line 26 "t_5_014.cpp"
//R true
#if __TESTWAVE_LONG_MAX__ <= __TESTWAVE_LONG_MIN__
    "Bad evaluation of long."
#else
true
#endif

//R #line 34 "t_5_014.cpp"
//R true
#if __TESTWAVE_LONG_MAX__ <= (__TESTWAVE_LONG_MAX__ / 2)  /* 0x3FFFFFFF   */
    "Bad evaluation of long."
#else
true
#endif

// 12.2:
//R #line 43 "t_5_014.cpp"
//R true
#if __TESTWAVE_ULONG_MAX__ / 2 < __TESTWAVE_LONG_MAX__
    "Bad evaluation of unsigned long."
#else
true
#endif

// 12.3: Octal number.
//R #line 52 "t_5_014.cpp"
//R true
#if 0177777 != 65535
    "Bad evaluation of octal number."
#else
true
#endif

// 12.4: Hexadecimal number.
//R #line 61 "t_5_014.cpp"
//R true
#if 0Xffff != 65535 || 0xFfFf != 65535
    "Bad evaluation of hexadecimal number."
#else
true
#endif

// 12.5: Suffix 'L' or 'l'.
//R #line 70 "t_5_014.cpp"
//R true
#if 0L != 0 || 0l != 0
    "Bad evaluation of 'L' suffix."
#else
true
#endif

// 12.6: Suffix 'U' or 'u'.
//R #line 79 "t_5_014.cpp"
//R true
#if 1U != 1 || 1u != 1
    "Bad evaluation of 'U' suffix."
#else
true
#endif

// 12.7: Negative integer.
//R #line 88 "t_5_014.cpp"
//R true
#if 0 <= -1
    "Bad evaluation of negative number."
#else
true
#endif

// 12.8: Long Long integers
//R #line 97 "t_5_014.cpp"
//R true
#if 0LL != 0 || 0ll != 0
    "Bad evaluation of 'LL' suffix."
#else
true
#endif

// 12.8: Unsigned Long Long integers
//R #line 106 "t_5_014.cpp"
//R true
#if 1ull != 1 || 1uLL != 1 || 1Ull != 1 || 1ULL != 1 || 1llu != 1 || 1llU != 1 || 1LLu != 1 || 1LLU != 1
    "Bad evaluation of 'ULL' or 'LLU' suffix."
#else
true
#endif

// 12.9: invalid (mixed case) long long integers
//R #line 115 "t_5_014.cpp"
//R long long foo = 1234l L;
//R long long bar = 5678L l;
//R unsigned long long baz = 1234uL l;
//R unsigned long long quux = 5678uL l;
long long foo = 1234lL;
long long bar = 5678Ll;
unsigned long long baz = 1234uLl;
unsigned long long quux = 5678uLl;


/*-
 * Copyright (c) 1998, 2002-2005 Kiyoshi Matsui <kmatsui@t3.rim.or.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

