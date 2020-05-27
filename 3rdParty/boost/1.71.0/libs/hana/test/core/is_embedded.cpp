// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>

#include <climits>
namespace hana = boost::hana;


// This test makes sure that fundamental types are properly embedded in
// each other, when sensible.

static_assert(hana::is_embedded<double, long double>{}, "");
static_assert(hana::is_embedded<float, long double>{}, "");
static_assert(hana::is_embedded<float, double>{}, "");

static_assert(hana::is_embedded<signed long,  signed long long>{}, "");
static_assert(hana::is_embedded<signed int,   signed long long>{}, "");
static_assert(hana::is_embedded<signed short, signed long long>{}, "");
static_assert(hana::is_embedded<signed char,  signed long long>{}, "");
static_assert(hana::is_embedded<signed int,   signed long>{}, "");
static_assert(hana::is_embedded<signed short, signed long>{}, "");
static_assert(hana::is_embedded<signed char,  signed long>{}, "");
static_assert(hana::is_embedded<signed short, signed int>{}, "");
static_assert(hana::is_embedded<signed char,  signed int>{}, "");
static_assert(hana::is_embedded<signed char,  signed short>{}, "");

static_assert(hana::is_embedded<unsigned long,  unsigned long long>{}, "");
static_assert(hana::is_embedded<unsigned int,   unsigned long long>{}, "");
static_assert(hana::is_embedded<unsigned short, unsigned long long>{}, "");
static_assert(hana::is_embedded<unsigned char,  unsigned long long>{}, "");
static_assert(hana::is_embedded<unsigned int,   unsigned long>{}, "");
static_assert(hana::is_embedded<unsigned short, unsigned long>{}, "");
static_assert(hana::is_embedded<unsigned char,  unsigned long>{}, "");
static_assert(hana::is_embedded<unsigned short, unsigned int>{}, "");
static_assert(hana::is_embedded<unsigned char,  unsigned int>{}, "");
static_assert(hana::is_embedded<unsigned char,  unsigned short>{}, "");

#if CHAR_MIN < 0 // char is signed

    static_assert(hana::is_embedded<char, signed long long>{}, "");
    static_assert(hana::is_embedded<char, signed long>{}, "");
    static_assert(hana::is_embedded<char, signed int>{}, "");
    static_assert(hana::is_embedded<char, signed short>{}, "");

    static_assert(hana::equal('a', static_cast<signed int>('a')), "");

#else // char is unsigned

    static_assert(hana::is_embedded<char, unsigned long long>{}, "");
    static_assert(hana::is_embedded<char, unsigned long>{}, "");
    static_assert(hana::is_embedded<char, unsigned int>{}, "");
    static_assert(hana::is_embedded<char, unsigned short>{}, "");

    static_assert(hana::equal('a', static_cast<unsigned int>('a')), "");

#endif


int main() { }
