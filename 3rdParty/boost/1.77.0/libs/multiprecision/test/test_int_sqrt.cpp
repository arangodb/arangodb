///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_int.hpp>
#include "test.hpp"

using Integer = boost::multiprecision::cpp_int;

void check_sqrt(const Integer& s, const Integer& r, const Integer& v) {
   BOOST_CHECK_EQUAL(s * s + r, v);
   BOOST_CHECK_GE(r, Integer(0));
   BOOST_CHECK_LE(s * s, v);
   BOOST_CHECK_GT((s + 1) * (s + 1), v);
}

template<typename I>
void check(const I& v) {
   I r;
   I s = boost::multiprecision::sqrt(v, r);
   check_sqrt(Integer(s), Integer(r), Integer(v));
}

void check_types(const Integer& v) {
   using namespace boost::multiprecision;
   size_t bits = 0;
   if (v > 0) {
      bits = msb(v);
   }
   if (bits < 32) {
      check(static_cast<uint32_t>(v));
   }
   if (bits < 64) {
      check(static_cast<uint64_t>(v));
   }
   if (bits < 128) {
      check(uint128_t(v));
   }
   if (bits < 256) {
      check(uint256_t(v));
   }
   if (bits < 512) {
      check(uint512_t(v));
   }
   check(v);
}

void check_near(const Integer& v) {
   check_types(v);
   for (size_t j = 0; j < 8; j++) {
      check_types(v - Integer(1 << j));
      check_types(v - Integer(1 << j) - 1);
      check_types(v - Integer(1 << j) + 1);
      check_types(v + Integer(1 << j));
      check_types(v + Integer(1 << j) - 1);
      check_types(v + Integer(1 << j) + 1);
   }
}

void test_first() {
   for (size_t i = 0; i < (1 << 16); i++) {
      check_types(Integer(i));
   }
}

void test_perfect() {
   for (size_t i = 256; i < (1 << 14); i++) {
      check_near(Integer(i) * Integer(i));
   }
}

void test_powers() {
   for (size_t i = 24; i < 2048; i++) {
      check_near(Integer(i) << i);
   }
}

void test_big() {
   for (size_t bits = 128; bits <= 2048; bits *= 2) {
      Integer i = Integer(1) << bits;
      Integer s = (i >> 8);
      Integer step = (i - s) / (1 << 8);
      for (Integer j = s; j <= i; j += step) {
         check_near(j);
      }
   }
}

int main()
{
   using namespace boost::multiprecision;

   test_first();
   test_perfect();
   test_powers();
   test_big();

   return boost::report_errors();
}
