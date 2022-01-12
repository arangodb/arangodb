///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_int.hpp>

#include "test_arithmetic.hpp"

template <unsigned MinBits, unsigned MaxBits, boost::multiprecision::cpp_integer_type SignType, class Allocator, boost::multiprecision::expression_template_option ExpressionTemplates>
struct is_twos_complement_integer<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<MinBits, MaxBits, SignType, boost::multiprecision::checked, Allocator>, ExpressionTemplates> > : public std::integral_constant<bool, false>
{};

template <>
struct related_type<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::uint128_t::backend_type> > >
{
   typedef boost::multiprecision::uint128_t type;
};


int main()
{
   test<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::uint128_t::backend_type> > >();
   return boost::report_errors();
}
