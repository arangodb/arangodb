//  (C) Copyright John Maddock 2016.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#if !defined(TEST_MPF_50) && !defined(TEST_MPF) && !defined(TEST_BACKEND) && !defined(TEST_CPP_DEC_FLOAT)\
   && !defined(TEST_MPFR) && !defined(TEST_MPFR_50) && !defined(TEST_MPFI_50) && !defined(TEST_FLOAT128)\
   && !defined(TEST_CPP_BIN_FLOAT) && !defined(TEST_CPP_DEC_FLOAT_2) && !defined(TEST_CPP_DEC_FLOAT_3)\
  && !defined(TEST_CPP_DEC_FLOAT_4) && !defined(TEST_CPP_DEC_FLOAT_5)
#  define TEST_MPF_50
#  define TEST_MPFR_50
#  define TEST_MPFI_50
#  define TEST_CPP_DEC_FLOAT
#  define TEST_CPP_DEC_FLOAT_2
#  define TEST_CPP_DEC_FLOAT_3
#  define TEST_CPP_DEC_FLOAT_4
#  define TEST_CPP_DEC_FLOAT_5
#  define TEST_FLOAT128
#  define TEST_CPP_BIN_FLOAT

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
#if defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_DEC_FLOAT_2) || defined(TEST_CPP_DEC_FLOAT_3) || defined(TEST_CPP_DEC_FLOAT_4) || defined(TEST_CPP_DEC_FLOAT_5)
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif
#ifdef TEST_CPP_BIN_FLOAT
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/debug_adaptor.hpp>
#endif
#ifdef TEST_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif

#include <boost/math/constants/constants.hpp>
#include "test.hpp"

#ifdef signbit
#undef signbit
#endif
#ifdef sign
#undef sign
#endif
#ifdef changesign
#undef changesign
#endif
#ifdef copysign
#undef copysign
#endif
#ifdef fpclassify
#undef fpclassify
#endif
#ifdef isinf
#undef isinf
#endif
#ifdef isnan
#undef isnan
#endif
#ifdef isnormal
#undef isnormal
#endif

#ifdef MPFR_VERSION_MAJOR
#define BOOST_MPFR_VERSION MPFR_VERSION_MAJOR * 10000 + MPFR_VERSION_MINOR * 100 + MPFR_VERSION_PATCHLEVEL
#endif

template <class T, class U>
void test_less(T a, U b)
{
   BOOST_CHECK(a < b);
   BOOST_CHECK(a <= b);
   BOOST_CHECK(!(a > b));
   BOOST_CHECK(!(a >= b));
   BOOST_CHECK(!(a == b));
   BOOST_CHECK((a != b));

   BOOST_CHECK(b > a);
   BOOST_CHECK(b >= a);
   BOOST_CHECK(!(b < a));
   BOOST_CHECK(!(b <= a));
   BOOST_CHECK(!(b == a));
   BOOST_CHECK((b != a));

   BOOST_CHECK(isless(a, b));
   BOOST_CHECK(islessequal(a, b));
   BOOST_CHECK(!isgreater(a, b));
   BOOST_CHECK(!isgreaterequal(a, b));
   BOOST_CHECK(islessgreater(a, b));

   BOOST_CHECK(!isless(b, a));
   BOOST_CHECK(!islessequal(b, a));
   BOOST_CHECK(isgreater(b, a));
   BOOST_CHECK(isgreaterequal(b, a));
   BOOST_CHECK(islessgreater(b, a));
}
template <class T, class U>
void test_equal(T a, U b)
{
   BOOST_CHECK(!(a < b));
   BOOST_CHECK(a <= b);
   BOOST_CHECK(!(a > b));
   BOOST_CHECK((a >= b));
   BOOST_CHECK((a == b));
   BOOST_CHECK(!(a != b));

   BOOST_CHECK(!(b > a));
   BOOST_CHECK(b >= a);
   BOOST_CHECK(!(b < a));
   BOOST_CHECK((b <= a));
   BOOST_CHECK((b == a));
   BOOST_CHECK(!(b != a));

   BOOST_CHECK(!isless(a, b));
   BOOST_CHECK(islessequal(a, b));
   BOOST_CHECK(!isgreater(a, b));
   BOOST_CHECK(isgreaterequal(a, b));
   BOOST_CHECK(!islessgreater(a, b));

   BOOST_CHECK(!isless(b, a));
   BOOST_CHECK(islessequal(b, a));
   BOOST_CHECK(!isgreater(b, a));
   BOOST_CHECK(isgreaterequal(b, a));
   BOOST_CHECK(!islessgreater(b, a));
}
template <class T, class U>
void test_unordered(T a, U b)
{
   BOOST_CHECK(!(a < b));
   BOOST_CHECK(!(a <= b));
   BOOST_CHECK(!(a > b));
   BOOST_CHECK(!(a >= b));
   BOOST_CHECK(!(a == b));
   BOOST_CHECK((a != b));

   BOOST_CHECK(!(b > a));
   BOOST_CHECK(!(b >= a));
   BOOST_CHECK(!(b < a));
   BOOST_CHECK(!(b <= a));
   BOOST_CHECK(!(b == a));
   BOOST_CHECK((b != a));

   BOOST_CHECK(!isless(a, b));
   BOOST_CHECK(!islessequal(a, b));
   BOOST_CHECK(!isgreater(a, b));
   BOOST_CHECK(!isgreaterequal(a, b));
   BOOST_CHECK(!islessgreater(a, b));

   BOOST_CHECK(!isless(b, a));
   BOOST_CHECK(!islessequal(b, a));
   BOOST_CHECK(!isgreater(b, a));
   BOOST_CHECK(!isgreaterequal(b, a));
   BOOST_CHECK(!islessgreater(b, a));
}

template <class T>
void test()
{
   //
   // Basic sanity checks for C99 functions which are just imported versions
   // from Boost.Math.  These should still be found via ADL so no using declarations here...
   //
   T val = 2;
   BOOST_CHECK(signbit(val) == 0);
   BOOST_CHECK(signbit(val + 2) == 0);
   val = -val;
   BOOST_CHECK(signbit(val));
   BOOST_CHECK(signbit(val * 2));

   val = 2;
   BOOST_CHECK_EQUAL(sign(val), 1);
   BOOST_CHECK_EQUAL(sign(val + 2), 1);
   val = -val;
   BOOST_CHECK_EQUAL(sign(val), -1);
   BOOST_CHECK_EQUAL(sign(val * 2), -1);
   val = 0;
   BOOST_CHECK_EQUAL(sign(val), 0);
   BOOST_CHECK_EQUAL(sign(val * 2), 0);

   val = 2;
   BOOST_CHECK_EQUAL(changesign(val), -2);
   BOOST_CHECK_EQUAL(changesign(val * 2), -4);
   val = -2;
   BOOST_CHECK_EQUAL(changesign(val), 2);
   BOOST_CHECK_EQUAL(changesign(val * 2), 4);
   val = 0;
   BOOST_CHECK_EQUAL(changesign(val), 0);
   BOOST_CHECK_EQUAL(changesign(val * 2), 0);
   // Things involving signed zero, need to detect it first:
   T neg_zero_test = -(std::numeric_limits<T>::min)();
   neg_zero_test /= (std::numeric_limits<T>::max)();
   T one(1);
   bool test_signed_zero = !boost::multiprecision::is_interval_number<T>::value && std::numeric_limits<T>::has_infinity && (one / neg_zero_test < 0);
   if(test_signed_zero)
   {
      BOOST_CHECK(signbit(changesign(val)));
      BOOST_CHECK(signbit(changesign(val * 2)));
   }

   T s = 2;
   val = 3;
   BOOST_CHECK_EQUAL(copysign(val, s), 3);
   BOOST_CHECK_EQUAL(copysign(val, s * -2), -3);
   BOOST_CHECK_EQUAL(copysign(-2 * val, s), 6);
   BOOST_CHECK_EQUAL(copysign(-2 * val, 2 * s), 6);
   s = -2;
   BOOST_CHECK_EQUAL(copysign(val, s), -3);
   BOOST_CHECK_EQUAL(copysign(val, s * -2), 3);
   BOOST_CHECK_EQUAL(copysign(-2 * val, s), -6);
   BOOST_CHECK_EQUAL(copysign(-2 * val, 2 * s), -6);
   val = -3;
   BOOST_CHECK_EQUAL(copysign(val, s), -3);
   BOOST_CHECK_EQUAL(copysign(val, s * -2), 3);
   BOOST_CHECK_EQUAL(copysign(-2 * val, s), -6);
   BOOST_CHECK_EQUAL(copysign(-2 * val, 2 * s), -6);
   s = 0;
   BOOST_CHECK_EQUAL(copysign(val, s), 3);
   // Things involving signed zero, need to detect it first:
   if(test_signed_zero)
   {
      BOOST_CHECK_EQUAL(copysign(val, s * -2), -3);
   }
   BOOST_CHECK_EQUAL(copysign(-2 * val, s), 6);
   BOOST_CHECK_EQUAL(copysign(-2 * val, 2 * s), 6);
   // Things involving signed zero, need to detect it first:
   if(test_signed_zero)
   {
      s = changesign(s);
      if(signbit(s))
      {
         BOOST_CHECK_EQUAL(copysign(val, s), -3);
         BOOST_CHECK_EQUAL(copysign(val, s * -2), 3);
         BOOST_CHECK_EQUAL(copysign(-2 * val, s), -6);
         BOOST_CHECK_EQUAL(copysign(-2 * val, 2 * s), -6);
      }
   }

   val = 3;
   BOOST_CHECK_EQUAL(fpclassify(val), FP_NORMAL);
   BOOST_CHECK_EQUAL(fpclassify(val * 3), FP_NORMAL);
   BOOST_CHECK(!isinf(val));
   BOOST_CHECK(!isinf(val + 2));
   BOOST_CHECK(!isnan(val));
   BOOST_CHECK(!isnan(val + 2));
   BOOST_CHECK(isnormal(val));
   BOOST_CHECK(isnormal(val + 2));
   val = -3;
   BOOST_CHECK_EQUAL(fpclassify(val), FP_NORMAL);
   BOOST_CHECK_EQUAL(fpclassify(val * 3), FP_NORMAL);
   BOOST_CHECK(!isinf(val));
   BOOST_CHECK(!isinf(val + 2));
   BOOST_CHECK(!isnan(val));
   BOOST_CHECK(!isnan(val + 2));
   BOOST_CHECK(isnormal(val));
   BOOST_CHECK(isnormal(val + 2));
   val = 0;
   BOOST_CHECK_EQUAL(fpclassify(val), FP_ZERO);
   BOOST_CHECK_EQUAL(fpclassify(val * 3), FP_ZERO);
   BOOST_CHECK(!isinf(val));
   BOOST_CHECK(!isinf(val + 2));
   BOOST_CHECK(!isnan(val));
   BOOST_CHECK(!isnan(val + 2));
   BOOST_CHECK(!isnormal(val));
   BOOST_CHECK(!isnormal(val * 2));
   BOOST_CHECK(!isnormal(val * -2));
   if(std::numeric_limits<T>::has_infinity)
   {
      val = std::numeric_limits<T>::infinity();
      BOOST_CHECK_EQUAL(fpclassify(val), FP_INFINITE);
      BOOST_CHECK_EQUAL(fpclassify(val * 3), FP_INFINITE);
      BOOST_CHECK(isinf(val));
      BOOST_CHECK(isinf(val + 2));
      BOOST_CHECK(!isnan(val));
      BOOST_CHECK(!isnan(val + 2));
      BOOST_CHECK(!isnormal(val));
      BOOST_CHECK(!isnormal(val + 2));
      val = -val;
      BOOST_CHECK_EQUAL(fpclassify(val), FP_INFINITE);
      BOOST_CHECK_EQUAL(fpclassify(val * 3), FP_INFINITE);
      BOOST_CHECK(isinf(val));
      BOOST_CHECK(isinf(val + 2));
      BOOST_CHECK(!isnan(val));
      BOOST_CHECK(!isnan(val + 2));
      BOOST_CHECK(!isnormal(val));
      BOOST_CHECK(!isnormal(val + 2));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      val = std::numeric_limits <T>::quiet_NaN();
      BOOST_CHECK_EQUAL(fpclassify(val), FP_NAN);
      BOOST_CHECK_EQUAL(fpclassify(val * 3), FP_NAN);
      BOOST_CHECK(!isinf(val));
      BOOST_CHECK(!isinf(val + 2));
      BOOST_CHECK(isnan(val));
      BOOST_CHECK(isnan(val + 2));
      BOOST_CHECK(!isnormal(val));
      BOOST_CHECK(!isnormal(val + 2));
   }
   s = 8 * std::numeric_limits<T>::epsilon();
   val = 2.5;
   BOOST_CHECK_CLOSE_FRACTION(asinh(val), T("1.6472311463710957106248586104436196635044144301932365282203100930843983757633104078778420255069424907777006132075516484778755360595913172299093829522950397895699619540523579875476513967578478619028438291006578604823887119907434"), s);
   BOOST_CHECK_CLOSE_FRACTION(asinh(val + T(0)), T("1.6472311463710957106248586104436196635044144301932365282203100930843983757633104078778420255069424907777006132075516484778755360595913172299093829522950397895699619540523579875476513967578478619028438291006578604823887119907434"), s);
   BOOST_CHECK_CLOSE_FRACTION(acosh(val), T("1.5667992369724110786640568625804834938620823510926588639329459980122148134693922696279968499622201141051039184050936311066453565386393240356562374302417843319480223211857615778787272615171906055455922537080327062362258846337050"), s);
   BOOST_CHECK_CLOSE_FRACTION(acosh(val + T(0)), T("1.5667992369724110786640568625804834938620823510926588639329459980122148134693922696279968499622201141051039184050936311066453565386393240356562374302417843319480223211857615778787272615171906055455922537080327062362258846337050"), s);
   val = 0.5;
   BOOST_CHECK_CLOSE_FRACTION(atanh(val), T("0.5493061443340548456976226184612628523237452789113747258673471668187471466093044834368078774068660443939850145329789328711840021129652599105264009353836387053015813845916906835896868494221804799518712851583979557605727959588753"), s);
   BOOST_CHECK_CLOSE_FRACTION(atanh(val + T(0)), T("0.5493061443340548456976226184612628523237452789113747258673471668187471466093044834368078774068660443939850145329789328711840021129652599105264009353836387053015813845916906835896868494221804799518712851583979557605727959588753"), s);
   val = 55.25;
   BOOST_CHECK_CLOSE_FRACTION(cbrt(val), T("3.8087058015466360309383876359583281382991983919300128125378938779672144843676192684301168479657279498120767424724024965319869248797423276064015643361426189576415670917818313417529572608229017809069355688606687557031643655896118"), s);
   BOOST_CHECK_CLOSE_FRACTION(cbrt(val + T(0)), T("3.8087058015466360309383876359583281382991983919300128125378938779672144843676192684301168479657279498120767424724024965319869248797423276064015643361426189576415670917818313417529572608229017809069355688606687557031643655896118"), s);
   if(!boost::multiprecision::is_interval_number<T>::value)
   {
      val = 2.75;
      BOOST_CHECK_CLOSE_FRACTION(erf(val), T("0.9998993780778803631630956080249130432349352621422640655161095794654526422025908961447328296681056892975214344779300734620255391682713519265048496199034963706976420982849598189071465666866369396765001072187538732800143945532487"), s);
      BOOST_CHECK_CLOSE_FRACTION(erf(val + T(0)), T("0.9998993780778803631630956080249130432349352621422640655161095794654526422025908961447328296681056892975214344779300734620255391682713519265048496199034963706976420982849598189071465666866369396765001072187538732800143945532487"), s);
      BOOST_CHECK_CLOSE_FRACTION(erfc(val), T("0.0001006219221196368369043919750869567650647378577359344838904205345473577974091038552671703318943107024785655220699265379744608317286480734951503800965036293023579017150401810928534333133630603234998927812461267199856054467512"), s);
      BOOST_CHECK_CLOSE_FRACTION(erfc(val + T(0)), T("0.0001006219221196368369043919750869567650647378577359344838904205345473577974091038552671703318943107024785655220699265379744608317286480734951503800965036293023579017150401810928534333133630603234998927812461267199856054467512"), s);
   }
   val = 0.125;
   BOOST_CHECK_CLOSE_FRACTION(expm1(val), T("0.1331484530668263168290072278117938725655031317451816259128200360788235778800483865139399907949417285732315270156473075657048210452584733998785564025916995261162759280700397984729320345630340659469435372721057879969170503978449"), s);
   BOOST_CHECK_CLOSE_FRACTION(expm1(val + T(0)), T("0.1331484530668263168290072278117938725655031317451816259128200360788235778800483865139399907949417285732315270156473075657048210452584733998785564025916995261162759280700397984729320345630340659469435372721057879969170503978449"), s);

   val = 20;
   s = 2;
   BOOST_CHECK_EQUAL(fdim(val, s), 18);
   BOOST_CHECK_EQUAL(fdim(s, val), 0);
   BOOST_CHECK_EQUAL(fdim(val, s * 2), 16);
   BOOST_CHECK_EQUAL(fdim(s * 2, val), 0);
   BOOST_CHECK_EQUAL(fdim(val, 2), 18);
   BOOST_CHECK_EQUAL(fdim(2, val), 0);

   BOOST_CHECK_EQUAL(fmax(val, s), val);
   BOOST_CHECK_EQUAL(fmax(s, val), val);
   BOOST_CHECK_EQUAL(fmax(val * 2, s), val * 2);
   BOOST_CHECK_EQUAL(fmax(val, s * 2), val);
   BOOST_CHECK_EQUAL(fmax(val * 2, s * 2), val * 2);
   BOOST_CHECK_EQUAL(fmin(val, s), s);
   BOOST_CHECK_EQUAL(fmin(s, val), s);
   BOOST_CHECK_EQUAL(fmin(val * 2, s), s);
   BOOST_CHECK_EQUAL(fmin(val, s * 2), s * 2);
   BOOST_CHECK_EQUAL(fmin(val * 2, s * 2), s * 2);

   BOOST_CHECK_EQUAL(fmax(val, 2), val);
   BOOST_CHECK_EQUAL(fmax(val, 2.0), val);
   BOOST_CHECK_EQUAL(fmax(20, s), val);
   BOOST_CHECK_EQUAL(fmax(20.0, s), val);
   BOOST_CHECK_EQUAL(fmin(val, 2), s);
   BOOST_CHECK_EQUAL(fmin(val, 2.0), s);
   BOOST_CHECK_EQUAL(fmin(20, s), s);
   BOOST_CHECK_EQUAL(fmin(20.0, s), s);
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      BOOST_CHECK_EQUAL(fmax(val, std::numeric_limits<T>::quiet_NaN()), val);
      BOOST_CHECK_EQUAL(fmax(std::numeric_limits<T>::quiet_NaN(), val), val);
      BOOST_CHECK_EQUAL(fmin(val, std::numeric_limits<T>::quiet_NaN()), val);
      BOOST_CHECK_EQUAL(fmin(std::numeric_limits<T>::quiet_NaN(), val), val);
   }
   if(std::numeric_limits<double>::has_quiet_NaN)
   {
      BOOST_CHECK_EQUAL(fmax(val, std::numeric_limits<double>::quiet_NaN()), val);
      BOOST_CHECK_EQUAL(fmax(std::numeric_limits<double>::quiet_NaN(), val), val);
      BOOST_CHECK_EQUAL(fmin(val, std::numeric_limits<double>::quiet_NaN()), val);
      BOOST_CHECK_EQUAL(fmin(std::numeric_limits<double>::quiet_NaN(), val), val);
   }

   test_less(s, val);
   test_less(2, val);
   test_less(s, 20);
   test_less(s + 0, val);
   test_less(s, val * 1);
   test_less(s * 1, val * 1);
   test_less(s * 1, 20);
   test_less(s + 2, val * 2);

   test_equal(val, val);
   test_equal(20, val);
   test_equal(val, 20);
   test_equal(val + 0, val);
   test_equal(val, val * 1);
   test_equal(val * 1, val * 1);
   test_equal(val * 1, 20);
   test_equal(val * 20, val * 20);

   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      s = std::numeric_limits<T>::quiet_NaN();
      test_unordered(s, val);
      test_unordered(s, 20);
      test_unordered(s + 0, val);
      test_unordered(s, val * 1);
      test_unordered(s * 1, val * 1);
      test_unordered(s * 1, 20);
      test_unordered(s + 2, val * 2);
      if(std::numeric_limits<double>::has_quiet_NaN)
      {
         double n = std::numeric_limits<double>::quiet_NaN();
         test_unordered(n, val);
      }
   }

   T tol = 8 * std::numeric_limits<T>::epsilon();
   s = 2;
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val, s)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val, 2)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val, 2.0)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(20, s)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(20.0, s)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val * 1, s)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val * 1, s * 1)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val * 1, 2)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(val * 1, 2.0)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(20, s * 1)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(hypot(20.0, s * 1)), T("20.099751242241780540438529825519152373890046940052754581145656594656982463103940762472355384907904704732599006530"), tol);

   BOOST_CHECK_CLOSE_FRACTION(lgamma(val), T("39.339884187199494036224652394567381081691457206897853119937969989377572554993874476249340525204204720861169039582"), tol);
   BOOST_CHECK_CLOSE_FRACTION(lgamma(val + 0), T("39.339884187199494036224652394567381081691457206897853119937969989377572554993874476249340525204204720861169039582"), tol);

   BOOST_CHECK_EQUAL(lrint(val), 20);
   BOOST_CHECK_EQUAL(lrint(val * 2), 40);
   BOOST_CHECK_EQUAL(llrint(val), 20);
   BOOST_CHECK_EQUAL(llrint(val * 2), 40);

   val = 0.125;
   BOOST_CHECK_CLOSE_FRACTION(log1p(val), T("0.117783035656383454538794109470521705068480712564733141107348638794807720528133786929641528638208114949935615070"), tol);
   BOOST_CHECK_CLOSE_FRACTION(log1p(val + 0), T("0.117783035656383454538794109470521705068480712564733141107348638794807720528133786929641528638208114949935615070"), tol);
   val = 20;
   BOOST_CHECK_CLOSE_FRACTION(T(log2(val)), T("4.321928094887362347870319429489390175864831393024580612054756395815934776608625215850139743359370155099657371710"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(log2(val + 0)), T("4.321928094887362347870319429489390175864831393024580612054756395815934776608625215850139743359370155099657371710"), tol);

   BOOST_CHECK_EQUAL(T(nearbyint(val)), 20);
   BOOST_CHECK_EQUAL(T(nearbyint(val + 0.25)), 20);
   BOOST_CHECK_EQUAL(T(rint(val)), 20);
   BOOST_CHECK_EQUAL(T(rint(val + 0.25)), 20);

   BOOST_CHECK_GT(nextafter(val, T(200)), val);
   BOOST_CHECK_GT(nextafter(val + 0, T(200)), val);
   BOOST_CHECK_GT(nextafter(val + 0, T(200) + 1), val);
   BOOST_CHECK_GT(nextafter(val, T(200) + 1), val);

   BOOST_CHECK_GT(nexttoward(val, T(200)), val);
   BOOST_CHECK_GT(nexttoward(val + 0, T(200)), val);
   BOOST_CHECK_GT(nexttoward(val + 0, T(200) + 1), val);
   BOOST_CHECK_GT(nexttoward(val, T(200) + 1), val);

   val = 21;
   s = 5;
   BOOST_CHECK_EQUAL(T(remainder(val, s)), 1);
   BOOST_CHECK_EQUAL(T(remainder(val, 5)), 1);
   BOOST_CHECK_EQUAL(T(remainder(21, s)), 1);
   BOOST_CHECK_EQUAL(T(remainder(val * 1, s)), 1);
   BOOST_CHECK_EQUAL(T(remainder(val * 1, s * 1)), 1);
   BOOST_CHECK_EQUAL(T(remainder(val, s * 1)), 1);
   BOOST_CHECK_EQUAL(T(remainder(val * 1, 5)), 1);
   BOOST_CHECK_EQUAL(T(remainder(21, s * 1)), 1);
   int i(0);
   BOOST_CHECK_EQUAL(T(remquo(val, s, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(val, 5, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(21, s, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(val * 1, s, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(val * 1, s * 1, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(val, s * 1, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(val * 1, 5, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   BOOST_CHECK_EQUAL(T(remquo(21, s * 1, &i)), 1);
   BOOST_CHECK_EQUAL(i, 4);
   i = 0;
   val = 5.25;
   tol = 3000;
   BOOST_CHECK_CLOSE_FRACTION(tgamma(val), T("35.211611852799685705225257690531248115026311138908448314086859575901217653313145619623624570033258659272301335544"), tol);
   BOOST_CHECK_CLOSE_FRACTION(tgamma(val + 1), T("184.86096222719834995243260287528905260388813347926935364895601277348139267989401450302402899267460796117958201160"), tol);

   BOOST_CHECK_CLOSE_FRACTION(T(exp2(val)), T("38.054627680087074134959999057935229289375106958842157216608071191022933383261349115865003025220405558913196632792"), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(exp2(val + 1)), T("76.109255360174148269919998115870458578750213917684314433216142382045866766522698231730006050440811117826393265585"), tol);
   val = 15;
   BOOST_CHECK_CLOSE_FRACTION(T(exp2(val)), T(32768uL), tol);
   BOOST_CHECK_CLOSE_FRACTION(T(exp2(val + 1)), T(65536uL), tol);

   i = fpclassify(val) + isgreaterequal(val, s) + islessequal(val, s) + isnan(val) + isunordered(val, s)
      + isfinite(val) + isinf(val) + islessgreater(val, s) + isnormal(val) + signbit(val) + isgreater(val, s) + isless(val, s);
}

template <class T>
void test_poison()
{
   // use these macros as proxies for determining C99 support:
#if defined(FP_ILOGB0) && defined(FP_INFINITE)
   //
   // These tests verify that our function overloads for Boost.Multiprecision
   // don't do anything nasty to the std:: overloads for built in types:
   //
   using namespace std;
   using namespace boost::multiprecision;
   //using namespace boost::math;

   T a(2), b(0.3f), c(4), result(0);
   int i;

   result += abs(a);
   result += cosh(a);
   result += fmod(a, b);
   result += logb(a);
   result += remquo(a, b, &i);
   result += acos(b);
   result += erf(a);
   result += frexp(a, &i);
   result += lrint(a);
   result += rint(a);
   result += acosh(b);
   result += erfc(b);
   result += hypot(a, b);
   result += lround(c);
   result += round(c);
   result += asin(b);
   result += exp2(a);
   result += ilogb(b);
   result += modf(a, &b);
   result += scalbln(a, i);
   result += asinh(b);
   result += exp(b);
   result += ldexp(a, i);
   result += scalbn(a, i);
   result += atan(b);
   result += expm1(a);
   result += lgamma(a);
   result += sin(b);
   result += atan2(a, c);
   result += fabs(a);
   result += llrint(a);
   result += sinh(b);
   result += atanh(b);
   result += fdim(a, b);
   result += llround(a);
   result += nearbyint(a);
   result += sqrt(b);
   result += cbrt(a);
   result += floor(b);
   result += log(a);
   result += nextafter(a, b);
   result += tan(b);
   result += ceil(b);
   result += fma(a, b, c);
   result += log10(a);
   result += nexttoward(a, b);
   result += tanh(a);
   result += copysign(a, b);
   result += fmax(a, b);
   result += log1p(a);
   result += pow(a, b);
   result += tgamma(a);
   result += cos(b);
   result += fmin(a, b);
   result += log2(a);
   result += remainder(a, b);
   result += trunc(b);
   result += min(a, b);
   result += max(a, b);

#if !BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION, < 60000)

   i = fpclassify(a) + isgreaterequal(a, b) + islessequal(a, b) + isnan(a) + isunordered(a, b)
      + isfinite(a) + isinf(a) + islessgreater(a, b) + isnormal(a) + signbit(a) + isgreater(a, b) + isless(a, b);
#endif
#endif
}

template <class T>
bool type_sets_errno(const T&)
{
   return true;
}
#ifdef TEST_MPFR_50
template<unsigned Digits10, boost::multiprecision::mpfr_allocation_type AllocateType, boost::multiprecision::expression_template_option ExpressionTemplates>
bool type_sets_errno(const boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<Digits10, AllocateType>, ExpressionTemplates> &)
{
   return false;
}
#endif
#ifdef TEST_FLOAT128
bool type_sets_errno(const boost::multiprecision::float128 &)
{
   return false;
}
#endif

template <class T>
typename boost::enable_if_c<std::numeric_limits<T>::is_specialized>::type check_invalid(const T& val)
{
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      BOOST_CHECK(isnan(val));
   }
   else
   {
      BOOST_CHECK_EQUAL(val, 0);
   }
   if(type_sets_errno(val))
      BOOST_CHECK_EQUAL(errno, EDOM);
   errno = 0;
}

template <class T>
typename boost::disable_if_c<std::numeric_limits<T>::is_specialized>::type check_invalid(const T& val)
{
   check_invalid(static_cast<typename T::result_type>(val));
}

template <class T>
void check_erange(const T& val)
{
   if(type_sets_errno(val))
      BOOST_CHECK_EQUAL(errno, ERANGE);
   errno = 0;
}

template <class T>
void test_c99_appendix_F()
{
   //
   // Tests conformance to non-normative appendix F.9.1 of C99, basically how to handle
   // special cases, infinities and NaN's.
   //
   errno = 0;
   T tol = std::numeric_limits<T>::epsilon();
   // F.9.1.1:
   T arg = 1;
   T val = acos(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = 2;
   check_invalid(acos(arg));
   arg = -2;
   check_invalid(acos(arg));
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      check_invalid(acos(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(acos(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(acos(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(acos(arg));
   }
   // F.9.1.2:
   arg = 0;
   val = asin(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = asin(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   arg = 2;
   check_invalid(asin(arg));
   arg = -2;
   check_invalid(asin(arg));
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      check_invalid(asin(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(asin(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(asin(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(asin(arg));
   }
   // F.9.1.3:
   arg = 0;
   val = atan(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = atan(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = atan(arg);
      BOOST_CHECK_EQUAL(val, boost::math::constants::half_pi<T>());
      arg = -std::numeric_limits<T>::infinity();
      val = atan(arg);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::half_pi<T>());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(asin(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(asin(arg));
   }
   // F.9.1.4:
   arg = 0;
   T arg2 = 0;
   val = atan2(arg, arg2);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   arg2 = -arg2;
   if(signbit(arg2))
   {
      arg = 0;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, boost::math::constants::pi<T>());
      BOOST_CHECK(signbit(val) == 0);
      arg = -arg;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::pi<T>());
      BOOST_CHECK(signbit(val));
   }
   arg = 0;
   arg2 = -2;
   val = atan2(arg, arg2);
   BOOST_CHECK_EQUAL(val, boost::math::constants::pi<T>());
   arg = -arg;
   if(signbit(arg))
   {
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::pi<T>());
   }
   arg = 0;
   arg2 = 2;
   val = atan2(arg, arg2);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   arg = -2;
   arg2 = 0;
   val = atan2(arg, arg2);
   BOOST_CHECK_EQUAL(val, -boost::math::constants::half_pi<T>());
   arg2 = -arg2;
   if(signbit(arg2))
   {
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::half_pi<T>());
   }
   arg = 2;
   arg2 = 0;
   val = atan2(arg, arg2);
   BOOST_CHECK_EQUAL(val, boost::math::constants::half_pi<T>());
   arg2 = -arg2;
   if(signbit(arg2))
   {
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, boost::math::constants::half_pi<T>());
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 2;
      arg2 = -std::numeric_limits<T>::infinity();
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, boost::math::constants::pi<T>());
      arg = -arg;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::pi<T>());
      arg = 2;
      arg2 = std::numeric_limits<T>::infinity();
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg = -arg;
      if(signbit(-T(0)))
      {
         val = atan2(arg, arg2);
         BOOST_CHECK_EQUAL(val, 0);
         BOOST_CHECK(signbit(val));
      }
      arg = std::numeric_limits<T>::infinity();
      arg2 = 2;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, boost::math::constants::half_pi<T>());
      arg = -arg;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::half_pi<T>());
      arg = std::numeric_limits<T>::infinity();
      arg2 = -2;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, boost::math::constants::half_pi<T>());
      arg = -arg;
      val = atan2(arg, arg2);
      BOOST_CHECK_EQUAL(val, -boost::math::constants::half_pi<T>());
      arg = std::numeric_limits<T>::infinity();
      arg2 = -std::numeric_limits<T>::infinity();
      val = atan2(arg, arg2);
      BOOST_CHECK_CLOSE_FRACTION(val, boost::math::constants::three_quarters_pi<T>(), tol);
      arg = -arg;
      val = atan2(arg, arg2);
      BOOST_CHECK_CLOSE_FRACTION(val, -boost::math::constants::three_quarters_pi<T>(), tol);
      arg = std::numeric_limits<T>::infinity();
      arg2 = std::numeric_limits<T>::infinity();
      val = atan2(arg, arg2);
      BOOST_CHECK_CLOSE_FRACTION(val, ldexp(boost::math::constants::pi<T>(), -2), tol);
      arg = -arg;
      val = atan2(arg, arg2);
      BOOST_CHECK_CLOSE_FRACTION(val, -ldexp(boost::math::constants::pi<T>(), -2), tol);
      if(std::numeric_limits<T>::has_quiet_NaN)
      {
         arg = std::numeric_limits<T>::quiet_NaN();
         arg2 = 2;
         check_invalid(atan2(arg, arg2));
         std::swap(arg, arg2);
         check_invalid(atan2(arg, arg2));
         arg = std::numeric_limits<T>::quiet_NaN();
         check_invalid(atan2(arg, arg2));
      }
   }
   // F.9.1.5:
   arg = 0;
   val = cos(arg);
   BOOST_CHECK_EQUAL(val, 1);
   arg = -arg;
   BOOST_CHECK_EQUAL(val, 1);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      check_invalid(cos(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(cos(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(cos(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(cos(arg));
   }
   // F.9.1.6:
   arg = 0;
   val = sin(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = sin(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      check_invalid(sin(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(sin(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(sin(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(sin(arg));
   }
   // F.9.1.7:
   arg = 0;
   val = tan(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = tan(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      check_invalid(tan(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(tan(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(tan(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(tan(arg));
   }
   // F.9.2.1:
   arg = 1;
   val = acosh(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   check_invalid(acosh(arg));
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = acosh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());

      arg = -std::numeric_limits<T>::infinity();
      check_invalid(acosh(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(acosh(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(acosh(arg));
   }
   // F.9.2.2:
   arg = 0;
   val = asinh(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = asinh(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = asinh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());

      arg = -std::numeric_limits<T>::infinity();
      val = asinh(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(asinh(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(asinh(arg));
   }
   // F.9.2.3:
   arg = 0;
   val = atanh(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = atanh(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   arg = 2;
   check_invalid(atanh(arg));
   arg = -3;
   check_invalid(atanh(arg));

   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 1;
      val = atanh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      BOOST_CHECK(signbit(val) == 0);
      check_erange(val);
      arg = -arg;
      val = atanh(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      BOOST_CHECK(signbit(val));
      check_erange(val);

      arg = std::numeric_limits<T>::infinity();
      check_invalid(atanh(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(atanh(arg));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(atanh(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(atanh(arg));
   }
   // F.9.2.4:
   arg = 0;
   val = cosh(arg);
   BOOST_CHECK_EQUAL(val, 1);
   arg = -arg;
   if(signbit(arg))
   {
      val = cosh(arg);
      BOOST_CHECK_EQUAL(val, 1);
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = (std::numeric_limits<T>::max)();
      val = cosh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = cosh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = std::numeric_limits<T>::infinity();
      val = cosh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = cosh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(cosh(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(cosh(arg));
   }
   // F.9.2.5:
   arg = 0;
   val = sinh(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = sinh(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = (std::numeric_limits<T>::max)();
      val = sinh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = sinh(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      arg = std::numeric_limits<T>::infinity();
      val = sinh(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = sinh(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(sinh(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(sinh(arg));
   }
   // F.9.2.6:
   arg = 0;
   val = tanh(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = tanh(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   arg = (std::numeric_limits<T>::max)();
   val = tanh(arg);
   BOOST_CHECK_EQUAL(val, 1);
   arg = -arg;
   val = tanh(arg);
   BOOST_CHECK_EQUAL(val, -1);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = tanh(arg);
      BOOST_CHECK_EQUAL(val, 1);
      arg = -arg;
      val = tanh(arg);
      BOOST_CHECK_EQUAL(val, -1);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(tanh(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(tanh(arg));
   }
   // F.9.3.1:
   arg = 0;
   val = exp(arg);
   BOOST_CHECK_EQUAL(val, 1);
   arg = -arg;
   if(signbit(arg))
   {
      val = exp(arg);
      BOOST_CHECK_EQUAL(val, 1);
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = exp(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = exp(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg = (std::numeric_limits<T>::max)();
      val = exp(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = exp(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(exp(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(exp(arg));
   }
   // F.9.3.2:
   arg = 0;
   val = exp2(arg);
   BOOST_CHECK_EQUAL(val, 1);
   arg = -arg;
   if(signbit(arg))
   {
      val = exp2(arg);
      BOOST_CHECK_EQUAL(val, 1);
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = exp2(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = exp2(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg = (std::numeric_limits<T>::max)();
      val = exp2(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = exp2(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(exp2(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(exp2(arg));
   }
   // F.9.3.3:
   arg = 0;
   val = expm1(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = expm1(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = expm1(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = expm1(arg);
      BOOST_CHECK_EQUAL(val, -1);
      arg = (std::numeric_limits<T>::max)();
      val = expm1(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = expm1(arg);
      BOOST_CHECK_EQUAL(val, -1);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(expm1(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(expm1(arg));
   }
   // F.9.3.4:
   arg = 0;
   int ival;
   val = frexp(arg, &ival);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK_EQUAL(ival, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = frexp(arg, &ival);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = frexp(arg, &ival);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = frexp(arg, &ival);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      val = frexp(arg, &ival);
      BOOST_CHECK(isnan(val));
   }
   // F.9.3.5:
   typename T::backend_type::exponent_type eval;
   typename T::backend_type::exponent_type fp_ilogb0 = (std::numeric_limits<typename T::backend_type::exponent_type>::min)();
   typename T::backend_type::exponent_type fp_ilogbnan =
#ifdef FP_ILOGBNAN
      FP_ILOGBNAN < 0 ? (std::numeric_limits<typename T::backend_type::exponent_type>::min)() : (std::numeric_limits<typename T::backend_type::exponent_type>::max)();
#else
      INT_MAX;
#endif

   arg = 0;
   eval = ilogb(arg);
   BOOST_CHECK_EQUAL(eval, fp_ilogb0);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      eval = ilogb(arg);
      BOOST_CHECK_EQUAL(eval, (std::numeric_limits<typename T::backend_type::exponent_type>::max)());
      arg = -arg;
      eval = ilogb(arg);
      BOOST_CHECK_EQUAL(eval, (std::numeric_limits<typename T::backend_type::exponent_type>::max)());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      eval = ilogb(arg);
      BOOST_CHECK_EQUAL(eval, fp_ilogbnan);
   }
   // F.9.3.7:
   arg = 1;
   val = log(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0;
      val = log(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = log(arg);
         BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = -1;
      check_invalid(log(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(log(arg));
      arg = std::numeric_limits<T>::infinity();
      val = log(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(log(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(log(arg));
   }
   // F.9.3.8:
   arg = 1;
   val = log10(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0;
      val = log10(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = log10(arg);
         BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = -1;
      check_invalid(log10(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(log10(arg));
      arg = std::numeric_limits<T>::infinity();
      val = log10(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(log10(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(log10(arg));
   }
   // F.9.3.9:
   arg = 0;
   val = log1p(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = log1p(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = -1;
      val = log1p(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -2;
      check_invalid(log1p(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(log1p(arg));
      arg = std::numeric_limits<T>::infinity();
      val = log1p(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(log1p(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(log1p(arg));
   }
   // F.9.3.10:
   arg = 1;
   val = log2(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0;
      val = log2(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = log2(arg);
         BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = -1;
      check_invalid(log2(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(log2(arg));
      arg = std::numeric_limits<T>::infinity();
      val = log2(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(log2(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(log2(arg));
   }
   // F.9.3.11:
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0;
      val = logb(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = logb(arg);
         BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = std::numeric_limits<T>::infinity();
      val = logb(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -std::numeric_limits<T>::infinity();
      val = logb(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(logb(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(logb(arg));
   }
   // F.9.3.13:
   arg = 0;
   val = scalbn(arg, 2);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = scalbn(arg, 2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = scalbn(arg, -100);
      BOOST_CHECK_EQUAL(val, arg);
      arg = -arg;
      val = scalbn(arg, -100);
      BOOST_CHECK_EQUAL(val, arg);
   }
   // F.9.4.1:
   arg = 0;
   val = cbrt(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = cbrt(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
#if !(defined(TEST_FLOAT128) && defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION < 40800))
   //
   // This test fails with early implementations of libquadmath - not our issue!
   //
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = cbrt(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -std::numeric_limits<T>::infinity();
      val = cbrt(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
#endif
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(cbrt(arg));
      arg = -std::numeric_limits<T>::quiet_NaN();
      check_invalid(cbrt(arg));
   }
   // F.9.4.2:
   arg = 0;
   val = fabs(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = fabs(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = fabs(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -std::numeric_limits<T>::infinity();
      val = fabs(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   // F.9.4.3:
   arg = 2;
   arg2 = 0;
   val = hypot(arg, arg2);
   BOOST_CHECK_EQUAL(val, arg);
   arg2 = -arg2;
   val = hypot(arg, arg2);
   BOOST_CHECK_EQUAL(val, arg);
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      arg2 = 2;
      val = hypot(arg, arg2);
      BOOST_CHECK_EQUAL(val, arg);
      arg = -arg;
      val = hypot(arg, arg2);
      BOOST_CHECK_EQUAL(val, -arg);
      arg2 = std::numeric_limits<T>::quiet_NaN();
      val = hypot(arg, arg2);
      BOOST_CHECK_EQUAL(val, -arg);
      arg = -arg;
      val = hypot(arg, arg2);
      BOOST_CHECK_EQUAL(val, arg);
   }
   // F.9.4.4:
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0;
      arg2 = -3;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = pow(arg, arg2);
         BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = 0;
      arg2 = -2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = pow(arg, arg2);
         BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = 0;
      arg2 = 3;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg = -arg;
      if(signbit(arg))
      {
         val = pow(arg, arg2);
         BOOST_CHECK_EQUAL(val, 0);
         BOOST_CHECK(signbit(val));
      }
      arg = 0;
      arg2 = 2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg = -arg;
      if(signbit(arg))
      {
         val = pow(arg, arg2);
         BOOST_CHECK_EQUAL(val, 0);
         BOOST_CHECK(signbit(val) == 0);
      }
      arg = -1;
      arg2 = std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = -std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg = 1;
      arg2 = 0;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = -std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = std::numeric_limits<T>::quiet_NaN();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg = 0;
      arg2 = 0;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = -arg2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg = std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = -arg2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg = std::numeric_limits<T>::quiet_NaN();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
      arg2 = -arg2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 1);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = -2.5;
      arg2 = 2.5;
      check_invalid(pow(arg, arg2));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0.5;
      arg2 = -std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -0.25;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = 2.5;
      arg2 = -std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      arg = -arg;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      arg = 2.5;
      arg2 = std::numeric_limits<T>::infinity();
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -std::numeric_limits<T>::infinity();
      arg2 = -3;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      if(signbit(-T(0)))
         BOOST_CHECK(signbit(val));
      arg2 = -2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg2 = -2.5;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg2 = 3;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
      arg2 = 2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg2 = 2.5;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg; // +INF
      arg2 = -2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg2 = -3;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg2 = -3.5;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg2 = 2;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg2 = 3;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg2 = 3.5;
      val = pow(arg, arg2);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   // F.9.4.5:
   arg = 0;
   val = sqrt(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = sqrt(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
#if !(defined(TEST_FLOAT128) && defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION < 40800))
   //
   // This test fails with early implementations of libquadmath - not our issue!
   //
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = sqrt(arg);
      BOOST_CHECK_EQUAL(val, arg);
      arg = -arg;
      check_invalid(sqrt(arg));
   }
#endif
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(sqrt(arg));
   }
   // F.9.5.1:
   arg = 0;
   val = erf(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = erf(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = erf(arg);
      BOOST_CHECK_EQUAL(val, 1);
      arg = -arg;
      val = erf(arg);
      BOOST_CHECK_EQUAL(val, -1);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(erf(arg));
   }
   // F.9.5.2:
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = erfc(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val) == 0);
      arg = -arg;
      val = erfc(arg);
      BOOST_CHECK_EQUAL(val, 2);
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(erfc(arg));
   }
   // F.9.5.3:
   arg = 1;
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = 2;
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
#if !defined(BOOST_MPFR_VERSION) || (BOOST_MPFR_VERSION > 30103)
   arg = 0;
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   check_erange(val);
   arg = -1;
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   check_erange(val);
   arg = -2;
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   check_erange(val);
   arg = -std::numeric_limits<T>::infinity();
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
#endif
   arg = std::numeric_limits<T>::infinity();
   val = lgamma(arg);
   BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(lgamma(arg));
   }
   // F.9.5.4:
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = 0;
      val = tgamma(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      check_erange(val);
      arg = -arg;
      if(signbit(arg))
      {
         val = tgamma(arg);
         BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
         check_erange(val);
      }
      arg = -1;
      check_invalid(tgamma(arg));
      arg = -std::numeric_limits<T>::infinity();
      check_invalid(tgamma(arg));
      arg = std::numeric_limits<T>::infinity();
      val = tgamma(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(tgamma(arg));
   }
   // F.9.6.1:
   arg = 0;
   val = ceil(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = ceil(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = ceil(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = ceil(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(ceil(arg));
   }
   // F.9.6.2:
   arg = 0;
   val = floor(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = floor(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = floor(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = floor(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(floor(arg));
   }
   // F.9.6.3:
   arg = 0;
   val = nearbyint(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = nearbyint(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = nearbyint(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = nearbyint(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(nearbyint(arg));
   }
   // F.9.6.4:
   arg = 0;
   val = rint(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = rint(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = rint(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = rint(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(rint(arg));
   }
   // F.9.6.6:
   arg = 0;
   val = round(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = round(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = round(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = round(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(round(arg));
   }
   // F.9.6.8:
   arg = 0;
   val = trunc(arg);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = trunc(arg);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      val = trunc(arg);
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      arg = -arg;
      val = trunc(arg);
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      check_invalid(trunc(arg));
   }
   // F.9.7.1:
   arg = 0;
   arg2 = 2;
   val = fmod(arg, arg2);
   BOOST_CHECK_EQUAL(val, 0);
   BOOST_CHECK(signbit(val) == 0);
   arg = -arg;
   if(signbit(arg))
   {
      val = fmod(arg, arg2);
      BOOST_CHECK_EQUAL(val, 0);
      BOOST_CHECK(signbit(val));
   }
   if(std::numeric_limits<T>::has_infinity)
   {
      arg = std::numeric_limits<T>::infinity();
      check_invalid(fmod(arg, arg2));
      arg = -arg;
      check_invalid(fmod(arg, arg2));
      arg = 2;
      arg2 = 0;
      check_invalid(fmod(arg, arg2));
      check_invalid(fmod(arg, -arg2));
   }
   if(std::numeric_limits<T>::has_quiet_NaN)
   {
      arg = std::numeric_limits<T>::quiet_NaN();
      arg2 = 2;
      check_invalid(fmod(arg, arg2));
      swap(arg, arg2);
      check_invalid(fmod(arg, arg2));
      check_invalid(fmod(arg2, arg2));
   }
}

int main()
{
   test_poison<float>();
   test_poison<double>();
#ifdef TEST_MPF_50
   test<boost::multiprecision::mpf_float_50>();
   test<boost::multiprecision::mpf_float_100>();
#endif
#ifdef TEST_MPFR_50
   std::cout << "Testing MPFR: " << MPFR_VERSION_STRING << std::endl;
   test<boost::multiprecision::mpfr_float_50>();
   test<boost::multiprecision::mpfr_float_100>();
   test_c99_appendix_F<boost::multiprecision::mpfr_float_50>();
#endif
#ifdef TEST_MPFI_50
   test<boost::multiprecision::mpfi_float_50>();
   test<boost::multiprecision::mpfi_float_100>();
#endif
#ifdef TEST_CPP_DEC_FLOAT
   test<boost::multiprecision::cpp_dec_float_50>();
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && defined(_WIN32)) // Object file too large otherwise
   test<boost::multiprecision::cpp_dec_float_100>();
#endif
   test_c99_appendix_F<boost::multiprecision::cpp_dec_float_50>();
#endif
#ifdef TEST_CPP_DEC_FLOAT_2
   // Some "peculiar" digit counts which stress our code:
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<65> > >();
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && defined(_WIN32)) // Object file too large otherwise
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<64> > >();
#endif
#endif
#ifdef TEST_CPP_DEC_FLOAT_3
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<63> > >();
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && defined(_WIN32)) // Object file too large otherwise
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<62> > >();
#endif
#endif
#ifdef TEST_CPP_DEC_FLOAT_4
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<61, long long> > >();
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && defined(_WIN32)) // Object file too large otherwise
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<60, long long> > >();
#endif
#endif
#ifdef TEST_CPP_DEC_FLOAT_5
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<59, long long, std::allocator<void> > > >();
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__GNUC__) && defined(_WIN32)) // Object file too large otherwise
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<58, long long, std::allocator<void> > > >();
#endif
#endif
#ifdef TEST_CPP_BIN_FLOAT
   test<boost::multiprecision::cpp_bin_float_50>();
   test<boost::multiprecision::number<boost::multiprecision::cpp_bin_float<100>, boost::multiprecision::et_on> >();
   test_c99_appendix_F<boost::multiprecision::cpp_bin_float_50>();
   test_c99_appendix_F<boost::multiprecision::number<boost::multiprecision::cpp_bin_float<100>, boost::multiprecision::et_on> >();
#endif
#ifdef TEST_FLOAT128
   test<boost::multiprecision::float128>();
   test_c99_appendix_F<boost::multiprecision::float128>();
#endif

   return boost::report_errors();
}


