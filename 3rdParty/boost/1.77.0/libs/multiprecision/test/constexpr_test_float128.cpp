//  (C) Copyright John Maddock 2019.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "constexpr_arithmetric_test.hpp"
#include <boost/multiprecision/float128.hpp>
#include <iostream>

int main()
{
   using boost::multiprecision::float128;

   {
      constexpr float128 a(22);
      constexpr float128 b = test_constexpr_add_subtract(a);

      constexpr __float128 f128 = (__float128)b;
      static_assert(f128 == -134.0f);

      constexpr int i = (int)b;
      static_assert(i == -134);

      constexpr short s = (short)b;
      static_assert(s == -134);
   }
   {
      constexpr float128 a(22);
      constexpr float128 b = test_constexpr_mul_divide(a);
      static_assert((__float128)b == 0);
   }
   {
      constexpr float128 a(22);
      constexpr float128 b = test_constexpr_compare(a);
      static_assert((__float128)b == 119);
   }
   {
      constexpr float128 a(0);
      static_assert(fpclassify(a) == FP_ZERO);
      constexpr float128 b(1);
      static_assert(fpclassify(b) == FP_NORMAL);
      constexpr float128 c(-1);
      static_assert(fpclassify(c) == FP_NORMAL);
      static_assert(abs(c) >= 0);
      static_assert(fabs(c) >= 0);
      constexpr float128 d(std::numeric_limits<float128>::epsilon());
      static_assert(fpclassify(c) == FP_NORMAL);
      constexpr float128 e((std::numeric_limits<float128>::min)());
      static_assert(fpclassify(e) == FP_NORMAL);
      constexpr float128 f((std::numeric_limits<float128>::max)());
      static_assert(fpclassify(f) == FP_NORMAL);
      constexpr float128 g(std::numeric_limits<float128>::lowest());
      static_assert(fpclassify(g) == FP_NORMAL);
      constexpr float128 h(std::numeric_limits<float128>::round_error());
      static_assert(fpclassify(h) == FP_NORMAL);
      constexpr float128 i(std::numeric_limits<float128>::denorm_min());
      static_assert(fpclassify(i) == FP_SUBNORMAL);
      constexpr float128 j(-std::numeric_limits<float128>::denorm_min());
      static_assert(fpclassify(j) == FP_SUBNORMAL);
      constexpr float128 k(std::numeric_limits<float128>::infinity());
      static_assert(fpclassify(k) == FP_INFINITE);
      static_assert(isinf(k));
      static_assert(!isnan(k));
      constexpr float128 l(-std::numeric_limits<float128>::infinity());
      static_assert(fpclassify(l) == FP_INFINITE);
      static_assert(isinf(l));
      static_assert(!isnan(l));
   }
   std::cout << "Done!" << std::endl;
}
