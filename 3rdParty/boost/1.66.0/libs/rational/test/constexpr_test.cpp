///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/rational.hpp>


int main()
{
#ifndef BOOST_NO_CXX11_CONSTEXPR
   constexpr boost::rational<int> i1;
   constexpr boost::rational<int> i2(3);
   constexpr boost::rational<long long> i3(i2);
   constexpr boost::rational<short> i4(i2);
   constexpr boost::rational<long long> i5(23u); // converting constructor
   // constexpr boost::rational<short> i6(23u); // Not supported, needs an explicit typecast in constructor.

   static_assert(i1.numerator() == 0, "constexpr test");
   static_assert(i1.denominator() == 1, "constexpr test");
   static_assert(i2.numerator() == 3, "constexpr test");
   static_assert(i2.denominator() == 1, "constexpr test");
   static_assert(i3.numerator() == 3, "constexpr test");
   static_assert(i3.denominator() == 1, "constexpr test");
   static_assert(!i1, "constexpr test");
   static_assert(i2, "constexpr test");
#endif
   return 0;
}
