//  Copyright John Maddock 2017.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Basic sanity check that header <boost/math/tools/config.hpp>
// #includes all the files that it needs to.
//
#include <boost/math/common_factor_ct.hpp>

template <boost::math::static_gcd_type a, boost::math::static_gcd_type b>
struct static_value_check;

template <boost::math::static_gcd_type a>
struct static_value_check<a, a>
{
   static const boost::math::static_gcd_type value = a;
};


void compile_and_link_test()
{
   typedef static_value_check<boost::math::static_gcd<42, 30>::value, 6> checked_type;
   boost::math::static_gcd_type result = checked_type::value;
   (void)result;
   typedef static_value_check<boost::math::static_lcm<18, 30>::value, 90> checked_type_2;
   boost::math::static_gcd_type result_2 = checked_type_2::value;
   (void)result_2;
}
