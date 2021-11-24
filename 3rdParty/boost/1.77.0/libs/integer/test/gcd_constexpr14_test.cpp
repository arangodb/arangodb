
//  (C) Copyright John Maddock 2017.
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  https://www.boost.org/LICENSE_1_0.txt)

#include <boost/integer/common_factor.hpp>

#ifndef BOOST_NO_CXX14_CONSTEXPR

void test_constexpr1()
{
   constexpr const boost::int64_t i = 347 * 463 * 727;
   constexpr const boost::int64_t j = 191 * 347 * 281;

   constexpr const boost::int64_t k = boost::integer::gcd(i, j);
   constexpr const boost::int64_t l = boost::integer::lcm(i, j);

   static_assert(k == 347, "Expected result not integer in constexpr gcd.");
   static_assert(l == 6268802158037, "Expected result not integer in constexpr lcm.");
}

void test_constexpr2()
{
   constexpr const boost::uint64_t i = 347 * 463 * 727;
   constexpr const boost::uint64_t j = 191 * 347 * 281;

   constexpr const boost::uint64_t k = boost::integer::gcd(i, j);
   constexpr const boost::uint64_t l = boost::integer::lcm(i, j);

   static_assert(k == 347, "Expected result not integer in constexpr gcd.");
   static_assert(l == 6268802158037, "Expected result not integer in constexpr lcm.");
}

void test_constexpr3()
{
   constexpr const boost::uint64_t i = 347 * 463 * 727;
   constexpr const boost::uint64_t j = 191 * 347 * 281;

   constexpr const boost::uint64_t k = boost::integer::gcd_detail::Euclid_gcd(i, j);

   static_assert(k == 347, "Expected result not integer in constexpr gcd.");
}

void test_constexpr4()
{
   constexpr const boost::uint64_t i = 347 * 463 * 727;
   constexpr const boost::uint64_t j = 191 * 347 * 281;

   constexpr const boost::uint64_t k = boost::integer::gcd_detail::mixed_binary_gcd(i, j);

   static_assert(k == 347, "Expected result not integer in constexpr gcd.");
}

void test_constexpr5()
{
   constexpr const boost::uint64_t i = 347 * 463 * 727;
   constexpr const boost::uint64_t j = 191 * 347 * 281;

   constexpr const boost::uint64_t k = boost::integer::gcd_detail::Stein_gcd(i, j);

   static_assert(k == 347, "Expected result not integer in constexpr gcd.");
}
#endif


