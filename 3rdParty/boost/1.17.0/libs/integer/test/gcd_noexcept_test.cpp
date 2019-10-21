
//  (C) Copyright John Maddock 2017.
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/integer/common_factor.hpp>

#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)
//
// These tests don't pass with GCC-4.x:
//
#if !defined(BOOST_GCC) || (BOOST_GCC >= 50000)

void test_noexcept(unsigned char a, unsigned char b)
{
   static_assert(noexcept(boost::integer::gcd(static_cast<unsigned char>(a), static_cast<unsigned char>(b))), "Expected a noexcept function.");
#ifndef _MSC_VER
   // This generates an internal compiler error if enabled as well as the following test:
   static_assert(noexcept(boost::integer::gcd(static_cast<char>(a), static_cast<char>(b))), "Expected a noexcept function.");
#endif
   static_assert(noexcept(boost::integer::gcd(static_cast<signed char>(a), static_cast<signed char>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<short>(a), static_cast<short>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<unsigned short>(a), static_cast<unsigned short>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<int>(a), static_cast<int>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<unsigned int>(a), static_cast<unsigned int>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<long>(a), static_cast<long>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<unsigned long>(a), static_cast<unsigned long>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<long long>(a), static_cast<long long>(b))), "Expected a noexcept function.");
   static_assert(noexcept(boost::integer::gcd(static_cast<unsigned long long>(a), static_cast<unsigned long long>(b))), "Expected a noexcept function.");
}

#endif
#endif

