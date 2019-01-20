// Copyright John Maddock 2013.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_bin_float.hpp>
#ifdef TEST_MPFR
#include <boost/multiprecision/mpfr.hpp>
#endif
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include "libs/multiprecision/test/test.hpp"
#include <iostream>
#include <iomanip>

template <class T>
T generate_random()
{
   typedef int e_type;
   static boost::random::mt19937 gen;
   T val = gen();
   T prev_val = -1;
   while(val != prev_val)
   {
      val *= (gen.max)();
      prev_val = val;
      val += gen();
   }
   e_type e;
   val = frexp(val, &e);

   static boost::random::uniform_int_distribution<e_type> ui(-20, 20);
   return ldexp(val, ui(gen));
}


using namespace boost::multiprecision;
#ifdef TEST_MPFR
typedef number<mpfr_float_backend<35> > good_type;
#else
typedef double good_type;
#endif
typedef number<cpp_bin_float<std::numeric_limits<good_type>::digits, digit_base_2>, et_off> test_type;

void test_special_cases()
{
   test_type max_val = (std::numeric_limits<test_type>::max)();
   test_type min_val = (std::numeric_limits<test_type>::min)();
   test_type eps = std::numeric_limits<test_type>::epsilon();
   test_type inf_val = (std::numeric_limits<test_type>::infinity)();
   test_type nan_val = (std::numeric_limits<test_type>::quiet_NaN)();
   test_type half = 0.5;
   test_type one_point_5 = 1.5;
   
   BOOST_CHECK((boost::math::isnormal)(max_val));
   BOOST_CHECK((boost::math::isnormal)(-max_val));
   BOOST_CHECK((boost::math::isnormal)(min_val));
   BOOST_CHECK((boost::math::isnormal)(-min_val));
   BOOST_CHECK((boost::math::isinf)(inf_val));
   BOOST_CHECK((boost::math::isinf)(-inf_val));
   BOOST_CHECK((boost::math::isnan)(nan_val));
   BOOST_CHECK((boost::math::isnan)(-nan_val));

   if(std::numeric_limits<test_type>::has_denorm)
      min_val = std::numeric_limits<test_type>::denorm_min();

   // Adding epsilon will increment 1.0:
   BOOST_CHECK(test_type(1) + eps != test_type(1));
   BOOST_CHECK(test_type(1) + eps / 2 == test_type(1));
   // But it's not the smallest value that will do that:
   test_type small = 1 + eps;
   small = ldexp(small, -std::numeric_limits<test_type>::digits);
   BOOST_CHECK(test_type(1) + small != test_type(1));
   // And if we increment 1.0 first, then an even smaller 
   // addition will round up:
   test_type one_next = test_type(1) + eps;
   BOOST_CHECK(one_next + eps / 2 != one_next);

   // Overflow:
   BOOST_CHECK_EQUAL(max_val + max_val * eps, inf_val);
   BOOST_CHECK_EQUAL(-max_val - max_val * eps, -inf_val);
   BOOST_CHECK_EQUAL(max_val * 2, inf_val);
   BOOST_CHECK_EQUAL(max_val * -2, -inf_val);
   BOOST_CHECK_EQUAL(max_val / half, inf_val);
   BOOST_CHECK_EQUAL(max_val / -half, -inf_val);
   BOOST_CHECK_EQUAL(max_val / min_val, inf_val);
   BOOST_CHECK_EQUAL(max_val / -min_val, -inf_val);
   // Underflow:
   BOOST_CHECK_EQUAL(min_val * 2 - one_point_5 * min_val, 0);
   BOOST_CHECK_EQUAL(-min_val * 2 + one_point_5 * min_val, 0);
   BOOST_CHECK_EQUAL(min_val / 2, 0);
   BOOST_CHECK_EQUAL(min_val / max_val, 0);
   BOOST_CHECK_EQUAL(min_val * half, 0);
   BOOST_CHECK_EQUAL(min_val - min_val, 0);
   BOOST_CHECK_EQUAL(max_val - max_val, 0);
   BOOST_CHECK_EQUAL(-min_val + min_val, 0);
   BOOST_CHECK_EQUAL(-max_val + max_val, 0);
   // Things which should not over/underflow:
   BOOST_CHECK_EQUAL((min_val * 2) / 2, min_val);
   BOOST_CHECK_EQUAL((max_val / 2) * 2, max_val);
   BOOST_CHECK_GE((min_val * 2.0000001) / 1.9999999999999999, min_val);
   BOOST_CHECK_LE((max_val / 2.0000001) * 1.9999999999999999, max_val);
   BOOST_CHECK_EQUAL(min_val * 2 - min_val, min_val);
   BOOST_CHECK_EQUAL(max_val / 2 + max_val / 2, max_val);
   // Things involving zero:
   BOOST_CHECK_EQUAL(max_val + 0, max_val);
   BOOST_CHECK_EQUAL(max_val - 0, max_val);
   BOOST_CHECK_EQUAL(0 + max_val, max_val);
   BOOST_CHECK_EQUAL(0 - max_val, -max_val);
   BOOST_CHECK_EQUAL(max_val * 0, 0);
   BOOST_CHECK_EQUAL(0 * max_val, 0);
   BOOST_CHECK_EQUAL(max_val / 0, inf_val);
   BOOST_CHECK_EQUAL(0 / max_val, 0);
   BOOST_CHECK_EQUAL(-max_val / 0, -inf_val);
   BOOST_CHECK_EQUAL(0 / -max_val, 0);
   // Things involving infinity:
   BOOST_CHECK_EQUAL(inf_val + 2, inf_val);
   BOOST_CHECK_EQUAL(inf_val - 2, inf_val);
   BOOST_CHECK_EQUAL(inf_val + -2, inf_val);
   BOOST_CHECK_EQUAL(inf_val - -2, inf_val);
   BOOST_CHECK_EQUAL(-inf_val + 2, -inf_val);
   BOOST_CHECK_EQUAL(-inf_val - 2, -inf_val);
   BOOST_CHECK_EQUAL(-inf_val + -2, -inf_val);
   BOOST_CHECK_EQUAL(-inf_val - -2, -inf_val);

   BOOST_CHECK_EQUAL(2 + inf_val, inf_val);
   BOOST_CHECK_EQUAL(2 - inf_val, -inf_val);
   BOOST_CHECK_EQUAL(-2 + inf_val, inf_val);
   BOOST_CHECK_EQUAL(-2 - inf_val, -inf_val);
   BOOST_CHECK_EQUAL(2 + (-inf_val), -inf_val);
   BOOST_CHECK_EQUAL(2 - (-inf_val), inf_val);
   BOOST_CHECK_EQUAL(-2 + (-inf_val), -inf_val);
   BOOST_CHECK_EQUAL(-2 - (-inf_val), inf_val);

   BOOST_CHECK_EQUAL(sqrt(inf_val), inf_val);
   BOOST_CHECK(boost::math::isnan(sqrt(-inf_val)));

   BOOST_CHECK_EQUAL(inf_val + test_type(2), inf_val);
   BOOST_CHECK_EQUAL(inf_val - test_type(2), inf_val);
   BOOST_CHECK_EQUAL(inf_val + test_type(-2), inf_val);
   BOOST_CHECK_EQUAL(inf_val - test_type(-2), inf_val);
   BOOST_CHECK_EQUAL(-inf_val + test_type(2), -inf_val);
   BOOST_CHECK_EQUAL(-inf_val - test_type(2), -inf_val);
   BOOST_CHECK_EQUAL(-inf_val + test_type(-2), -inf_val);
   BOOST_CHECK_EQUAL(-inf_val - test_type(-2), -inf_val);

   BOOST_CHECK_EQUAL(test_type(2) + inf_val, inf_val);
   BOOST_CHECK_EQUAL(test_type(2) - inf_val, -inf_val);
   BOOST_CHECK_EQUAL(test_type(-2) + inf_val, inf_val);
   BOOST_CHECK_EQUAL(test_type(-2) - inf_val, -inf_val);
   BOOST_CHECK_EQUAL(test_type(2) + (-inf_val), -inf_val);
   BOOST_CHECK_EQUAL(test_type(2) - (-inf_val), inf_val);
   BOOST_CHECK_EQUAL(test_type(-2) + (-inf_val), -inf_val);
   BOOST_CHECK_EQUAL(test_type(-2) - (-inf_val), inf_val);

   BOOST_CHECK((boost::math::isnan)(inf_val - inf_val));
   BOOST_CHECK_EQUAL(inf_val * 2, inf_val);
   BOOST_CHECK_EQUAL(-inf_val * 2, -inf_val);
   BOOST_CHECK_EQUAL(inf_val * -2, -inf_val);
   BOOST_CHECK_EQUAL(-inf_val * -2, inf_val);
   BOOST_CHECK_EQUAL(inf_val * test_type(-2), -inf_val);
   BOOST_CHECK_EQUAL(-inf_val * test_type(-2), inf_val);
   BOOST_CHECK((boost::math::isnan)(inf_val * 0));
   BOOST_CHECK((boost::math::isnan)(-inf_val * 0));
   BOOST_CHECK_EQUAL(inf_val / 2, inf_val);
   BOOST_CHECK_EQUAL(-inf_val / 2, -inf_val);
   BOOST_CHECK_EQUAL(inf_val / -2, -inf_val);
   BOOST_CHECK_EQUAL(-inf_val / -2, inf_val);
   BOOST_CHECK_EQUAL(inf_val / test_type(-2), -inf_val);
   BOOST_CHECK_EQUAL(-inf_val / test_type(-2), inf_val);
   BOOST_CHECK_EQUAL(inf_val / 0, inf_val);
   BOOST_CHECK_EQUAL(-inf_val / 0, -inf_val);
   BOOST_CHECK((boost::math::isnan)(inf_val / inf_val));
   BOOST_CHECK((boost::math::isnan)(-inf_val / inf_val));
   // Things involving nan:
   BOOST_CHECK((boost::math::isnan)(nan_val + 2));
   BOOST_CHECK((boost::math::isnan)(nan_val - 2));
   BOOST_CHECK((boost::math::isnan)(nan_val + 0));
   BOOST_CHECK((boost::math::isnan)(nan_val - 0));
   BOOST_CHECK((boost::math::isnan)(nan_val + inf_val));
   BOOST_CHECK((boost::math::isnan)(nan_val - inf_val));
   BOOST_CHECK((boost::math::isnan)(nan_val + nan_val));
   BOOST_CHECK((boost::math::isnan)(nan_val - nan_val));
   BOOST_CHECK((boost::math::isnan)(2 + nan_val));
   BOOST_CHECK((boost::math::isnan)(2 - nan_val));
   BOOST_CHECK((boost::math::isnan)(0 - nan_val));
   BOOST_CHECK((boost::math::isnan)(0 - nan_val));
   BOOST_CHECK((boost::math::isnan)(inf_val + nan_val));
   BOOST_CHECK((boost::math::isnan)(inf_val - nan_val));
   BOOST_CHECK((boost::math::isnan)(nan_val * 2));
   BOOST_CHECK((boost::math::isnan)(nan_val / 2));
   BOOST_CHECK((boost::math::isnan)(nan_val * 0));
   BOOST_CHECK((boost::math::isnan)(nan_val / 0));
   BOOST_CHECK((boost::math::isnan)(nan_val * inf_val));
   BOOST_CHECK((boost::math::isnan)(nan_val / inf_val));
   BOOST_CHECK((boost::math::isnan)(nan_val * nan_val));
   BOOST_CHECK((boost::math::isnan)(nan_val / nan_val));
   BOOST_CHECK((boost::math::isnan)(2 * nan_val));
   BOOST_CHECK((boost::math::isnan)(2 / nan_val));
   BOOST_CHECK((boost::math::isnan)(0 / nan_val));
   BOOST_CHECK((boost::math::isnan)(0 / nan_val));
   BOOST_CHECK((boost::math::isnan)(inf_val * nan_val));
   BOOST_CHECK((boost::math::isnan)(inf_val / nan_val));
   // Corner cases:
   BOOST_CHECK_EQUAL((max_val * half) / half, max_val);
   BOOST_CHECK_EQUAL((max_val / 2) * 2, max_val);
   BOOST_CHECK_EQUAL((min_val / half) * half, min_val);
   BOOST_CHECK_EQUAL((min_val * 2) / 2, min_val);
   BOOST_CHECK_EQUAL(max_val + min_val, max_val);
   BOOST_CHECK_EQUAL(min_val + max_val, max_val);
   BOOST_CHECK_EQUAL(max_val - min_val, max_val);
   BOOST_CHECK_EQUAL(min_val - max_val, -max_val);
   // Signed zeros:
   BOOST_CHECK(boost::math::signbit(min_val * -min_val));
   BOOST_CHECK(boost::math::signbit(min_val * min_val) == 0);
   BOOST_CHECK(boost::math::signbit(-min_val * -min_val) == 0);
   BOOST_CHECK(boost::math::signbit(-min_val * min_val));
   BOOST_CHECK(boost::math::signbit(min_val / max_val) == 0);
   BOOST_CHECK(boost::math::signbit(min_val / -max_val));
   BOOST_CHECK(boost::math::signbit(-min_val / -max_val) == 0);
   BOOST_CHECK(boost::math::signbit(-min_val / max_val));
   BOOST_CHECK(boost::math::signbit(min_val / 2) == 0);
   BOOST_CHECK(boost::math::signbit(min_val / -2));
   BOOST_CHECK(boost::math::signbit(-min_val / -2) == 0);
   BOOST_CHECK(boost::math::signbit(-min_val / 2));
   test_type neg_zero = min_val * -min_val;
   test_type zero = 0;
   // Arithmetic involving signed zero:
   BOOST_CHECK_EQUAL(-neg_zero, 0);
   BOOST_CHECK(!boost::math::signbit(-neg_zero));
   BOOST_CHECK_EQUAL(neg_zero + 2, 2);
   BOOST_CHECK_EQUAL(neg_zero + test_type(2), 2);
   BOOST_CHECK_EQUAL(2 + neg_zero, 2);
   BOOST_CHECK_EQUAL(test_type(2) + neg_zero, 2);
   BOOST_CHECK_EQUAL(neg_zero + -2, -2);
   BOOST_CHECK_EQUAL(neg_zero + test_type(-2), -2);
   BOOST_CHECK_EQUAL(-2 + neg_zero, -2);
   BOOST_CHECK_EQUAL(test_type(-2) + neg_zero, -2);
   BOOST_CHECK_EQUAL(neg_zero - 2, -2);
   BOOST_CHECK_EQUAL(neg_zero - test_type(2), -2);
   BOOST_CHECK_EQUAL(2 - neg_zero, 2);
   BOOST_CHECK_EQUAL(test_type(2) - neg_zero, 2);
   BOOST_CHECK_EQUAL(neg_zero - -2, 2);
   BOOST_CHECK_EQUAL(neg_zero - test_type(-2), 2);
   BOOST_CHECK_EQUAL(-2 - neg_zero, -2);
   BOOST_CHECK_EQUAL(test_type(-2) - neg_zero, -2);
   BOOST_CHECK(!boost::math::signbit(test_type(2) + test_type(-2)));
   BOOST_CHECK(!boost::math::signbit(test_type(2) - test_type(2)));
   BOOST_CHECK(!boost::math::signbit(test_type(-2) - test_type(-2)));
   BOOST_CHECK(!boost::math::signbit(test_type(-2) + test_type(2)));
   BOOST_CHECK(!boost::math::signbit(zero + zero));
   BOOST_CHECK(!boost::math::signbit(zero - zero));
   BOOST_CHECK(!boost::math::signbit(neg_zero + zero));
   BOOST_CHECK(!boost::math::signbit(zero + neg_zero));
   BOOST_CHECK(boost::math::signbit(neg_zero + neg_zero));
   BOOST_CHECK(boost::math::signbit(neg_zero - zero));
   BOOST_CHECK(!boost::math::signbit(zero - neg_zero));
   BOOST_CHECK(!boost::math::signbit(neg_zero - neg_zero));
   small = 0.25;
   BOOST_CHECK(!boost::math::signbit(floor(small)));
   BOOST_CHECK(!boost::math::signbit(round(small)));
   BOOST_CHECK(!boost::math::signbit(trunc(small)));
   small = -small;
   BOOST_CHECK(boost::math::signbit(ceil(small)));
   BOOST_CHECK(boost::math::signbit(round(small)));
   BOOST_CHECK(boost::math::signbit(trunc(small)));


   BOOST_CHECK_EQUAL(neg_zero * 2, 0);
   BOOST_CHECK_EQUAL(neg_zero * test_type(2), 0);
   BOOST_CHECK_EQUAL(2 * neg_zero, 0);
   BOOST_CHECK_EQUAL(test_type(2) * neg_zero, 0);
   BOOST_CHECK_EQUAL(neg_zero * -2, 0);
   BOOST_CHECK_EQUAL(neg_zero * test_type(-2), 0);
   BOOST_CHECK_EQUAL(-2 * neg_zero, 0);
   BOOST_CHECK_EQUAL(test_type(-2) * neg_zero, 0);
   BOOST_CHECK(boost::math::signbit(neg_zero * 2));
   BOOST_CHECK(boost::math::signbit(neg_zero * test_type(2)));
   BOOST_CHECK(boost::math::signbit(2 * neg_zero));
   BOOST_CHECK(boost::math::signbit(test_type(2) * neg_zero));
   BOOST_CHECK(!boost::math::signbit(neg_zero * -2));
   BOOST_CHECK(!boost::math::signbit(neg_zero * test_type(-2)));
   BOOST_CHECK(!boost::math::signbit(-2 * neg_zero));
   BOOST_CHECK(!boost::math::signbit(test_type(-2) * neg_zero));

   BOOST_CHECK_EQUAL(neg_zero / 2, 0);
   BOOST_CHECK_EQUAL(neg_zero / test_type(2), 0);
   BOOST_CHECK_EQUAL(2 / neg_zero, -inf_val);
   BOOST_CHECK_EQUAL(test_type(2) / neg_zero, -inf_val);
   BOOST_CHECK_EQUAL(neg_zero / -2, 0);
   BOOST_CHECK_EQUAL(neg_zero / test_type(-2), 0);
   BOOST_CHECK_EQUAL(-2 / neg_zero, inf_val);
   BOOST_CHECK_EQUAL(test_type(-2) / neg_zero, inf_val);
   BOOST_CHECK(boost::math::signbit(neg_zero / 2));
   BOOST_CHECK(boost::math::signbit(neg_zero / test_type(2)));
   BOOST_CHECK(boost::math::signbit(2 / neg_zero));
   BOOST_CHECK(boost::math::signbit(test_type(2) / neg_zero));
   BOOST_CHECK(!boost::math::signbit(neg_zero / -2));
   BOOST_CHECK(!boost::math::signbit(neg_zero / test_type(-2)));
   BOOST_CHECK(!boost::math::signbit(-2 / neg_zero));
   BOOST_CHECK(!boost::math::signbit(test_type(-2) / neg_zero));

   BOOST_CHECK(boost::math::signbit(neg_zero.convert_to<double>()));
   BOOST_CHECK(boost::math::signbit(neg_zero.convert_to<float>()));
   BOOST_CHECK(boost::math::signbit(neg_zero.convert_to<long double>()));
   BOOST_CHECK(!boost::math::signbit(zero.convert_to<double>()));
   BOOST_CHECK(!boost::math::signbit(zero.convert_to<float>()));
   BOOST_CHECK(!boost::math::signbit(zero.convert_to<long double>()));

   // Conversions to other types of special values:
   if(std::numeric_limits<float>::has_infinity)
   {
      BOOST_CHECK_EQUAL(inf_val.convert_to<float>(), std::numeric_limits<float>::infinity());
      BOOST_CHECK_EQUAL((-inf_val).convert_to<float>(), -std::numeric_limits<float>::infinity());
   }
   if(std::numeric_limits<float>::has_quiet_NaN)
   {
      BOOST_CHECK((boost::math::isnan)(nan_val.convert_to<float>()));
   }
   if(std::numeric_limits<double>::has_infinity)
   {
      BOOST_CHECK_EQUAL(inf_val.convert_to<double>(), std::numeric_limits<double>::infinity());
      BOOST_CHECK_EQUAL((-inf_val).convert_to<double>(), -std::numeric_limits<double>::infinity());
   }
   if(std::numeric_limits<double>::has_quiet_NaN)
   {
      BOOST_CHECK((boost::math::isnan)(nan_val.convert_to<double>()));
   }
   if(std::numeric_limits<long double>::has_infinity)
   {
      BOOST_CHECK_EQUAL(inf_val.convert_to<long double>(), std::numeric_limits<long double>::infinity());
      BOOST_CHECK_EQUAL((-inf_val).convert_to<long double>(), -std::numeric_limits<long double>::infinity());
   }
   if(std::numeric_limits<long double>::has_quiet_NaN)
   {
      BOOST_CHECK((boost::math::isnan)(nan_val.convert_to<long double>()));
   }
   //
   // Bug https://svn.boost.org/trac/boost/attachment/ticket/12580
   //
   using std::ldexp;
   test_type a(1);
   test_type b = ldexp(test_type(0.99), -std::numeric_limits<test_type>::digits);
   good_type ga(1);
   good_type gb = ldexp(good_type(0.99), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(0.5), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(0.5), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(1), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(1), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(0.50000000001), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(0.50000000001), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   a = a + ldexp(a, -20);
   ga = ga + ldexp(ga, -20);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(0.5), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(0.5), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(1), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(1), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(0.50000000001), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(0.50000000001), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   a = 1;
   a = boost::math::float_prior(a);
   ga = 1;
   ga = boost::math::float_prior(ga);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(0.5), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(0.5), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(1), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(1), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));

   b = ldexp(test_type(0.50000000001), -std::numeric_limits<test_type>::digits);
   gb = ldexp(good_type(0.50000000001), -std::numeric_limits<good_type>::digits);
   BOOST_CHECK_EQUAL(good_type(test_type(a - b)), good_type(ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - a)), good_type(gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + b)), good_type(ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + a)), good_type(gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(a - -b)), good_type(ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b - -a)), good_type(gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(a + -b)), good_type(ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(b + -a)), good_type(gb + -ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - b)), good_type(-ga - gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - a)), good_type(-gb - ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + b)), good_type(-ga + gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + a)), good_type(-gb + ga));

   BOOST_CHECK_EQUAL(good_type(test_type(-a - -b)), good_type(-ga - -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b - -a)), good_type(-gb - -ga));
   BOOST_CHECK_EQUAL(good_type(test_type(-a + -b)), good_type(-ga + -gb));
   BOOST_CHECK_EQUAL(good_type(test_type(-b + -a)), good_type(-gb + -ga));
}

int main()
{
   test_special_cases();
   unsigned error_count = 0;
   for(unsigned i = 0; i < 100000; ++i)
   {
      good_type a = generate_random<good_type>();
      good_type b = generate_random<good_type>();
      test_type ta(a);
      test_type tb(b);

      BOOST_CHECK_EQUAL(test_type(a * b), ta * tb);
      BOOST_CHECK_EQUAL(test_type(-a * b), -ta * tb);
      BOOST_CHECK_EQUAL(test_type(a * -b), ta * -tb);
      BOOST_CHECK_EQUAL(test_type(-a * -b), -ta * -tb);

      BOOST_CHECK_EQUAL(test_type(a + b), ta + tb);
      BOOST_CHECK_EQUAL(test_type(-a + b), -ta + tb);
      BOOST_CHECK_EQUAL(test_type(a + -b), ta + -tb);
      BOOST_CHECK_EQUAL(test_type(-a + -b), -ta + -tb);

      BOOST_CHECK_EQUAL(test_type(a - b), ta - tb);
      BOOST_CHECK_EQUAL(test_type(-a - b), -ta - tb);
      BOOST_CHECK_EQUAL(test_type(a - -b), ta - -tb);
      BOOST_CHECK_EQUAL(test_type(-a - -b), -ta - -tb);

      BOOST_CHECK_EQUAL(test_type(a / b), ta / tb);
      BOOST_CHECK_EQUAL(test_type(-a / b), -ta / tb);
      BOOST_CHECK_EQUAL(test_type(a / -b), ta / -tb);
      BOOST_CHECK_EQUAL(test_type(-a / -b), -ta / -tb);

      BOOST_CHECK_EQUAL(test_type(sqrt(a)), sqrt(ta));
      BOOST_CHECK_EQUAL(test_type(floor(a)), floor(ta));
      BOOST_CHECK_EQUAL(test_type(floor(-a)), floor(-ta));
      BOOST_CHECK_EQUAL(test_type(ceil(a)), ceil(ta));
      BOOST_CHECK_EQUAL(test_type(ceil(-a)), ceil(-ta));

#ifdef TEST_MPFR
      //
      // Conversions:
      //
      BOOST_CHECK_EQUAL(a.convert_to<double>(), ta.convert_to<double>());
      BOOST_CHECK_EQUAL(a.convert_to<float>(), ta.convert_to<float>());
      BOOST_CHECK_EQUAL(b.convert_to<double>(), tb.convert_to<double>());
      BOOST_CHECK_EQUAL(b.convert_to<float>(), tb.convert_to<float>());
#else
      BOOST_CHECK_EQUAL(a, ta.convert_to<double>());
      BOOST_CHECK_EQUAL(static_cast<float>(a), ta.convert_to<float>());
      BOOST_CHECK_EQUAL(b, tb.convert_to<double>());
      BOOST_CHECK_EQUAL(static_cast<float>(b), tb.convert_to<float>());
#endif

      static boost::random::mt19937 i_gen;

      int si = i_gen();
      BOOST_CHECK_EQUAL(test_type(a * si), ta * si);
      BOOST_CHECK_EQUAL(test_type(-a * si), -ta * si);
      BOOST_CHECK_EQUAL(test_type(-a * -si), -ta * -si);
      BOOST_CHECK_EQUAL(test_type(a * -si), ta * -si);
      unsigned ui = std::abs(si);
      BOOST_CHECK_EQUAL(test_type(a * ui), ta * ui);
      BOOST_CHECK_EQUAL(test_type(-a * ui), -ta * ui);

      // Divide:
      BOOST_CHECK_EQUAL(test_type(a / si), ta / si);
      BOOST_CHECK_EQUAL(test_type(-a / si), -ta / si);
      BOOST_CHECK_EQUAL(test_type(-a / -si), -ta / -si);
      BOOST_CHECK_EQUAL(test_type(a / -si), ta / -si);
      BOOST_CHECK_EQUAL(test_type(a / ui), ta / ui);
      BOOST_CHECK_EQUAL(test_type(-a / ui), -ta / ui);
      // Error reporting:
      if(boost::detail::test_errors() != error_count)
      {
         error_count = boost::detail::test_errors();
         std::cout << std::setprecision(std::numeric_limits<test_type>::max_digits10) << std::scientific;
         std::cout << "a (mpfr) = " << a << std::endl;
         std::cout << "a (test) = " << ta << std::endl;
         std::cout << "b (mpfr) = " << b << std::endl;
         std::cout << "b (test) = " << tb << std::endl;
         std::cout << "si       = " << si << std::endl;
         std::cout << "ui       = " << ui << std::endl;
      }
   }
   return boost::report_errors();
}

