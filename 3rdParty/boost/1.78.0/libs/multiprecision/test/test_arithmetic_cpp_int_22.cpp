///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_int.hpp>

#include "test_arithmetic.hpp"

template <>
struct related_type<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_uint256_t::backend_type> > >
{
   typedef boost::multiprecision::checked_uint256_t type;
};

template <>
struct is_twos_complement_integer<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_uint256_t::backend_type> > > : public std::false_type
{};

template <>
struct is_checked_cpp_int<boost::multiprecision::checked_uint256_t> : public std::integral_constant<bool, true>
{};
template <>
struct is_checked_cpp_int<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_uint256_t::backend_type> > > : public std::integral_constant<bool, true>
{};



int main()
{
   test<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::checked_uint256_t::backend_type> > >();
   return boost::report_errors();
}
