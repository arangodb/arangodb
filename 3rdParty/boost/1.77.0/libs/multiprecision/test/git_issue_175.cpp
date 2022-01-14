///////////////////////////////////////////////////////////////////////////////
//  Copyright 2019 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include "test.hpp"

using namespace boost::multiprecision;

template <class B, expression_template_option ET, class T>
void check_type_is_number(const number<B, ET>& v, T t) 
{
   BOOST_CHECK_EQUAL(v, t);
}

template <class T, class U>
void check_type_is_number(const T&, U)
{
   static_assert(sizeof(T) == 1, "Oooops we appear to have an expression template when we should not have done so!");
}

int main()
{
   cpp_dec_float_50 a(1), b(2), c(3);
   check_type_is_number(-cpp_dec_float_50(3), -3);
   check_type_is_number(a + cpp_dec_float_50(3), 4);
   check_type_is_number(a - cpp_dec_float_50(3), -2);
   check_type_is_number(a * cpp_dec_float_50(3), 3);
   check_type_is_number(c / cpp_dec_float_50(3), 1);

   check_type_is_number(cpp_dec_float_50(3) + a, 4);
   check_type_is_number(cpp_dec_float_50(3) - a, 2);
   check_type_is_number(cpp_dec_float_50(3) * a, 3);
   check_type_is_number(cpp_dec_float_50(3) / a, 3);

   check_type_is_number(3 + cpp_dec_float_50(3), 6);
   check_type_is_number(3 - cpp_dec_float_50(3), 0);
   check_type_is_number(3 * cpp_dec_float_50(3), 9);
   check_type_is_number(3 / cpp_dec_float_50(3), 1);

   check_type_is_number(cpp_dec_float_50(3) + 3, 6);
   check_type_is_number(cpp_dec_float_50(3) - 3, 0);
   check_type_is_number(cpp_dec_float_50(3) * 3, 9);
   check_type_is_number(cpp_dec_float_50(3) / 3, 1);
   
   check_type_is_number(-cpp_int(2), -2);
   check_type_is_number(~cpp_int(2), -3);
   check_type_is_number(cpp_int(2) % 3, 2);
   check_type_is_number(2 % cpp_int(3), 2);
   check_type_is_number(2 | cpp_int(3), 2|3);
   check_type_is_number(cpp_int(3)|2, 2|3);
   check_type_is_number(2 & cpp_int(3), 2&3);
   check_type_is_number(cpp_int(3)&2, 2&3);
   check_type_is_number(2 ^ cpp_int(3), 2^3);
   check_type_is_number(cpp_int(3)^2, 2^3);
   check_type_is_number(cpp_int(1) << 4, 1 << 4);
   check_type_is_number(cpp_int(4) >> 1, 2);

   check_type_is_number(asin(cpp_dec_float_50(0)), 0);
   check_type_is_number(pow(cpp_dec_float_50(2), 2), 4);
   check_type_is_number(pow(cpp_dec_float_50(2), b), 4);
   check_type_is_number(pow(2, cpp_dec_float_50(2)), 4);
   check_type_is_number(pow(b, cpp_dec_float_50(2)), 4);
   check_type_is_number(pow(cpp_dec_float_50(2), cpp_dec_float_50(2)), 4);

   return boost::report_errors();
}
