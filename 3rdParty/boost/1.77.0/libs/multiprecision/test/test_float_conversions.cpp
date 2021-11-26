///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include "test.hpp"

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/math/constants/constants.hpp>

int main()
{
   using namespace boost::multiprecision;

   static_assert((std::is_convertible<float, cpp_bin_float_single>::value), "Error check");
   static_assert(!(std::is_convertible<double, cpp_bin_float_single>::value), "Error check");
   static_assert(!(std::is_convertible<long double, cpp_bin_float_single>::value), "Error check");

   cpp_bin_float_single s = boost::math::constants::pi<cpp_bin_float_single>();
   std::cout << s << std::endl;

   typedef number<backends::cpp_bin_float<11, backends::digit_base_2, void, boost::int8_t, -14, 15>, et_off> cpp_bin_float_half;

   static_assert(!(std::is_convertible<float, cpp_bin_float_half>::value), "Error check");
   static_assert(!(std::is_convertible<double, cpp_bin_float_half>::value), "Error check");
   static_assert(!(std::is_convertible<long double, cpp_bin_float_half>::value), "Error check");
#ifdef BOOST_HAS_FLOAT128
   static_assert(!(std::is_convertible<__float128, cpp_bin_float_half>::value), "Error check");
#endif

   cpp_bin_float_half hs = boost::math::constants::pi<cpp_bin_float_half>();
   std::cout << hs << std::endl;

   return boost::report_errors();
}
