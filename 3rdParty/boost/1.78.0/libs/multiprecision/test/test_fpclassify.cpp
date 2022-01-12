//  Copyright John Maddock 2006.
//  Copyright Paul A. Bristow 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <cmath>
#include <math.h>
#include <boost/limits.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include "test.hpp"

#if !defined(TEST_MPF_50) && !defined(TEST_MPF) && !defined(TEST_BACKEND) && !defined(TEST_MPZ) && !defined(TEST_CPP_DEC_FLOAT) && !defined(TEST_MPFR) && !defined(TEST_MPFR_50) && !defined(TEST_MPQ) && !defined(TEST_MPFI_50) && !defined(TEST_FLOAT128) && !defined(TEST_CPP_BIN_FLOAT)
#define TEST_MPF_50
#define TEST_MPFR_50
#define TEST_MPFI_50
#define TEST_BACKEND
#define TEST_CPP_DEC_FLOAT
#define TEST_FLOAT128
#define TEST_CPP_BIN_FLOAT

#ifdef _MSC_VER
#pragma message("CAUTION!!: No backend type specified so testing everything.... this will take some time!!")
#endif
#ifdef __GNUC__
#pragma warning "CAUTION!!: No backend type specified so testing everything.... this will take some time!!"
#endif

#endif

#if defined(TEST_MPF_50)
#include <boost/multiprecision/gmp.hpp>
#endif
#ifdef TEST_MPFR_50
#include <boost/multiprecision/mpfr.hpp>
#endif
#ifdef TEST_MPFI_50
#include <boost/multiprecision/mpfi.hpp>
#endif
#ifdef TEST_BACKEND
#include <boost/multiprecision/concepts/mp_number_archetypes.hpp>
#endif
#ifdef TEST_CPP_DEC_FLOAT
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif
#ifdef TEST_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif
#ifdef TEST_CPP_BIN_FLOAT
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4127) //  conditional expression is constant
#endif

const char* method_name(const boost::math::detail::native_tag&)
{
   return "Native";
}

const char* method_name(const boost::math::detail::generic_tag<true>&)
{
   return "Generic (with numeric limits)";
}

const char* method_name(const boost::math::detail::generic_tag<false>&)
{
   return "Generic (without numeric limits)";
}

const char* method_name(const boost::math::detail::ieee_tag&)
{
   return "IEEE std";
}

const char* method_name(const boost::math::detail::ieee_copy_all_bits_tag&)
{
   return "IEEE std, copy all bits";
}

const char* method_name(const boost::math::detail::ieee_copy_leading_bits_tag&)
{
   return "IEEE std, copy leading bits";
}

template <class T>
void test()
{
   T t = 2;
   T u = 2;
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_NORMAL);
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_NORMAL);
   BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
   BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
   BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), true);
   BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), true);
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
   if (std::numeric_limits<T>::is_specialized)
   {
      t = (std::numeric_limits<T>::max)();
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_NORMAL);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_NORMAL);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
      t = (std::numeric_limits<T>::min)();
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_NORMAL);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_NORMAL);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
   }
   if (std::numeric_limits<T>::has_denorm)
   {
      t = (std::numeric_limits<T>::min)();
      t /= 2;
      if (t != 0)
      {
         BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_SUBNORMAL);
         BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_SUBNORMAL);
         BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
         BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
         BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
         BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
         BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
      }
      t = std::numeric_limits<T>::denorm_min();
      if ((t != 0) && (t < (std::numeric_limits<T>::min)()))
      {
         BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_SUBNORMAL);
         BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_SUBNORMAL);
         BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
         BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
         BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
         BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
         BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
         BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
      }
   }
   else
   {
      std::cout << "Denormalised forms not tested" << std::endl;
   }
   t = 0;
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_ZERO);
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_ZERO);
   BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
   BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
   BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
   t /= -u; // create minus zero if it exists
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_ZERO);
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_ZERO);
   BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), true);
   BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), true);
   BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
   BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
   BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
   // infinity:
   if (std::numeric_limits<T>::has_infinity)
   {
      // At least one std::numeric_limits<T>::infinity)() returns zero
      // (Compaq true64 cxx), hence the check.
      t = (std::numeric_limits<T>::infinity)();
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_INFINITE);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_INFINITE);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
#if !defined(BOOST_BORLANDC) && !(defined(__DECCXX) && !defined(_IEEE_FP))
      // divide by zero on Borland triggers a C++ exception :-(
      // divide by zero on Compaq CXX triggers a C style signal :-(
      t = 2;
      u = 0;
      t /= u;
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_INFINITE);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_INFINITE);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
      t = -2;
      t /= u;
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_INFINITE);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_INFINITE);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (::boost::math::fpclassify)(t + 0));
#else
      std::cout << "Infinities from divide by zero not tested" << std::endl;
#endif
   }
   else
   {
      std::cout << "Infinity not tested" << std::endl;
   }
#ifndef BOOST_BORLANDC
   // NaN's:
   // Note that Borland throws an exception if we even try to obtain a Nan
   // by calling std::numeric_limits<T>::quiet_NaN() !!!!!!!
   if (std::numeric_limits<T>::has_quiet_NaN)
   {
      t = std::numeric_limits<T>::quiet_NaN();
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_NAN);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_NAN);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
   }
   else
   {
      std::cout << "Quiet NaN's not tested" << std::endl;
   }
   if (std::numeric_limits<T>::has_signaling_NaN)
   {
      t = std::numeric_limits<T>::signaling_NaN();
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(t), (int)FP_NAN);
      BOOST_CHECK_EQUAL((::boost::math::fpclassify)(-t), (int)FP_NAN);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isfinite)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isinf)(-t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnan)(-t), true);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(t), false);
      BOOST_CHECK_EQUAL((::boost::math::isnormal)(-t), false);
   }
   else
   {
      std::cout << "Signaling NaN's not tested" << std::endl;
   }
#endif
   //
   // Try sign manipulation functions as well:
   //
   T one(1), minus_one(-1), zero(0);
   BOOST_CHECK((::boost::math::sign)(one) > 0);
   BOOST_CHECK((::boost::math::sign)(minus_one) < 0);
   BOOST_CHECK((::boost::math::sign)(zero) == 0);
   BOOST_CHECK((::boost::math::sign)(one + 2) > 0);
   BOOST_CHECK((::boost::math::sign)(minus_one - 30) < 0);
   BOOST_CHECK((::boost::math::sign)(-zero) == 0);

   BOOST_CHECK((::boost::math::signbit)(one) == 0);
   BOOST_CHECK((::boost::math::signbit)(minus_one) > 0);
   BOOST_CHECK((::boost::math::signbit)(zero) == 0);
   BOOST_CHECK((::boost::math::signbit)(one + 2) == 0);
   BOOST_CHECK((::boost::math::signbit)(minus_one - 30) > 0);
   //BOOST_CHECK((::boost::math::signbit)(-zero) == 0);

   BOOST_CHECK((::boost::math::signbit)(boost::math::changesign(one)) > 0);
   BOOST_CHECK_EQUAL(boost::math::changesign(one), minus_one);
   BOOST_CHECK((::boost::math::signbit)(boost::math::changesign(minus_one)) == 0);
   BOOST_CHECK_EQUAL(boost::math::changesign(minus_one), one);
   //BOOST_CHECK((::boost::math::signbit)(zero) == 0);
   BOOST_CHECK((::boost::math::signbit)(boost::math::changesign(one + 2)) > 0);
   BOOST_CHECK_EQUAL(boost::math::changesign(one + 2), -3);
   BOOST_CHECK((::boost::math::signbit)(boost::math::changesign(minus_one - 30)) == 0);
   BOOST_CHECK_EQUAL(boost::math::changesign(minus_one - 30), 31);
   //BOOST_CHECK((::boost::math::signbit)(-zero) == 0);

   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(one, one)) == 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(one, one), one);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(one, minus_one)) > 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(one, minus_one), minus_one);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(minus_one, one)) == 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(minus_one, one), one);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(minus_one, minus_one)) > 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(minus_one, minus_one), minus_one);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(one + 1, one + 2)) == 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(one + 1, one + 2), 2);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(one + 30, minus_one - 20)) > 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(one + 30, minus_one - 20), -31);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(minus_one + 2, one + 2)) == 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(minus_one - 2, one + 2), 3);
   BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(minus_one - 20, minus_one - 30)) > 0);
   BOOST_CHECK_EQUAL(boost::math::copysign(minus_one - 20, minus_one - 30), -21);

   // Things involving signed zero, need to detect it first:
   T neg_zero_test = -(std::numeric_limits<T>::min)();
   neg_zero_test /= (std::numeric_limits<T>::max)();
   if (std::numeric_limits<T>::has_infinity && (one / neg_zero_test < 0))
   {
#ifndef TEST_MPFI_50
      // Note that testing this with mpfi is in the "too difficult" drawer at present.
      std::cout << neg_zero_test << std::endl;
      BOOST_CHECK_EQUAL(neg_zero_test, 0);
      BOOST_CHECK((::boost::math::sign)(neg_zero_test) == 0);
      // We got -INF, so we have a signed zero:
      BOOST_CHECK((::boost::math::signbit)(neg_zero_test) > 0);
      BOOST_CHECK((::boost::math::signbit)(boost::math::changesign(zero)) > 0);
      BOOST_CHECK_EQUAL(boost::math::changesign(zero), 0);
      BOOST_CHECK((::boost::math::signbit)(boost::math::changesign(neg_zero_test)) == 0);
      BOOST_CHECK_EQUAL(boost::math::changesign(neg_zero_test), 0);
      BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(zero, one)) == 0);
      BOOST_CHECK_EQUAL(boost::math::copysign(zero, one), 0);
      BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(zero, minus_one)) > 0);
      BOOST_CHECK_EQUAL(boost::math::copysign(zero, minus_one), 0);
      BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(neg_zero_test, one)) == 0);
      BOOST_CHECK_EQUAL(boost::math::copysign(neg_zero_test, one), 0);
      BOOST_CHECK((::boost::math::signbit)(boost::math::copysign(neg_zero_test, minus_one)) > 0);
      BOOST_CHECK_EQUAL(boost::math::copysign(neg_zero_test, minus_one), 0);
#endif
   }
}

int main()
{
   BOOST_MATH_CONTROL_FP;
   // start by printing some information:
#ifdef isnan
   std::cout << "Platform has isnan macro." << std::endl;
#endif
#ifdef fpclassify
   std::cout << "Platform has fpclassify macro." << std::endl;
#endif
#ifdef BOOST_HAS_FPCLASSIFY
   std::cout << "Platform has FP_NORMAL macro." << std::endl;
#endif
   std::cout << "FP_ZERO: " << (int)FP_ZERO << std::endl;
   std::cout << "FP_NORMAL: " << (int)FP_NORMAL << std::endl;
   std::cout << "FP_INFINITE: " << (int)FP_INFINITE << std::endl;
   std::cout << "FP_NAN: " << (int)FP_NAN << std::endl;
   std::cout << "FP_SUBNORMAL: " << (int)FP_SUBNORMAL << std::endl;

#ifdef TEST_MPF_50
   test<boost::multiprecision::mpf_float_50>();
   test<boost::multiprecision::mpf_float_100>();
#endif
#ifdef TEST_MPFR_50
   test<boost::multiprecision::mpfr_float_50>();
   test<boost::multiprecision::mpfr_float_100>();
#endif
#ifdef TEST_MPFI_50
   test<boost::multiprecision::mpfi_float_50>();
   test<boost::multiprecision::mpfi_float_100>();
#endif
#ifdef TEST_CPP_DEC_FLOAT
   test<boost::multiprecision::cpp_dec_float_50>();
   test<boost::multiprecision::cpp_dec_float_100>();
#endif
#ifdef TEST_BACKEND
   test<boost::multiprecision::number<boost::multiprecision::concepts::number_backend_float_architype> >();
#endif
#ifdef TEST_FLOAT128
   test<boost::multiprecision::float128>();
#endif
#ifdef TEST_CPP_BIN_FLOAT
   test<boost::multiprecision::cpp_bin_float_50>();
   test<boost::multiprecision::number<boost::multiprecision::cpp_bin_float<35, boost::multiprecision::digit_base_10, std::allocator<char>, boost::long_long_type> > >();
#endif
   return boost::report_errors();
}
