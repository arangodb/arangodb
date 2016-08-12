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

   BOOST_CHECK(test_type(1) + eps != test_type(1));
   BOOST_CHECK(test_type(1) + eps / 2 == test_type(1));

   // Overflow:
   BOOST_CHECK_EQUAL(max_val + max_val * eps, inf_val);
   BOOST_CHECK_EQUAL(-max_val - max_val * eps, -inf_val);
   BOOST_CHECK_EQUAL(max_val * 2, inf_val);
   BOOST_CHECK_EQUAL(max_val * -2, -inf_val);
   BOOST_CHECK_EQUAL(max_val / half, inf_val);
   BOOST_CHECK_EQUAL(max_val / -half, -inf_val);
   // Underflow:
   BOOST_CHECK_EQUAL(min_val * 2 - one_point_5 * min_val, 0);
   BOOST_CHECK_EQUAL(-min_val * 2 + one_point_5 * min_val, 0);
   BOOST_CHECK_EQUAL(min_val / 2, 0);
   BOOST_CHECK_EQUAL(min_val * half, 0);
   BOOST_CHECK_EQUAL(min_val - min_val, 0);
   BOOST_CHECK_EQUAL(max_val - max_val, 0);
   BOOST_CHECK_EQUAL(-min_val + min_val, 0);
   BOOST_CHECK_EQUAL(-max_val + max_val, 0);
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
   BOOST_CHECK_EQUAL(inf_val + inf_val, inf_val);
   BOOST_CHECK_EQUAL(-inf_val - 2, -inf_val);
   BOOST_CHECK_EQUAL(inf_val - 2, inf_val);
   BOOST_CHECK_EQUAL(-inf_val + 2, -inf_val);
   BOOST_CHECK((boost::math::isnan)(inf_val - inf_val));
   BOOST_CHECK_EQUAL(inf_val * 2, inf_val);
   BOOST_CHECK_EQUAL(-inf_val * 2, -inf_val);
   BOOST_CHECK((boost::math::isnan)(inf_val * 0));
   BOOST_CHECK((boost::math::isnan)(-inf_val * 0));
   BOOST_CHECK_EQUAL(inf_val / 2, inf_val);
   BOOST_CHECK_EQUAL(-inf_val / 2, -inf_val);
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

