///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_bin_float.hpp>

#include "libs/multiprecision/test/test_arithmetic.hpp"

template <unsigned Digits, boost::multiprecision::backends::digit_base_type DigitBase, class Allocator, class Exponent, Exponent MinExponent, Exponent MaxExponent, boost::multiprecision::expression_template_option ET>
struct related_type<boost::multiprecision::number< boost::multiprecision::cpp_bin_float<Digits, DigitBase, Allocator, Exponent, MinExponent, MaxExponent>, ET> >
{
   typedef boost::multiprecision::number< boost::multiprecision::cpp_bin_float<Digits, DigitBase, Allocator, Exponent, MinExponent, MaxExponent>, ET> number_type;
   typedef boost::multiprecision::number< boost::multiprecision::cpp_bin_float<((std::numeric_limits<number_type>::digits / 2) > std::numeric_limits<long double>::digits ? Digits / 2 : Digits), DigitBase, Allocator, Exponent, MinExponent, MaxExponent>, ET> type;
};

int main()
{
   //test<boost::multiprecision::cpp_bin_float_50>();
   //test<boost::multiprecision::number<boost::multiprecision::cpp_bin_float<1000, boost::multiprecision::digit_base_10, std::allocator<char> > > >();
   test<boost::multiprecision::cpp_bin_float_quad>();
   return boost::report_errors();
}

