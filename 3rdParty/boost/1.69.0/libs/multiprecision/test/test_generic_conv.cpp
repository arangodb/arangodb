///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include "test.hpp"

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/random.hpp>

#ifdef TEST_GMP
#include <boost/multiprecision/gmp.hpp>
#endif
#ifdef TEST_TOMMATH
#include <boost/multiprecision/tommath.hpp>
#endif
#ifdef TEST_MPFR
#include <boost/multiprecision/mpfr.hpp>
#endif

int main()
{
   using namespace boost::multiprecision;
   using namespace boost::random;

   independent_bits_engine<mt11213b, 1024, cpp_int> gen;
   mt11213b small_gen;

   for(unsigned i = 0; i < 100; ++i)
   {
      cpp_int c = gen();
      //
      // Integer to integer conversions first:
      //
#ifdef TEST_GMP
      mpz_int z(c);
      cpp_int t(z);
      BOOST_CHECK_EQUAL(t, c);
      z.assign(-c);
      t.assign(-z);
      BOOST_CHECK_EQUAL(t, c);
#endif
#ifdef TEST_TOMMATH
      tom_int tom(c);
      cpp_int t2(tom);
      BOOST_CHECK_EQUAL(t2, c);
      tom.assign(-c);
      t2.assign(-tom);
      BOOST_CHECK_EQUAL(t2, c);
#endif
      //
      // Now integer to float:
      //
      typedef number<cpp_dec_float<500> > dec_float_500;
      dec_float_500 df(c);
      dec_float_500 df2(c.str());
      BOOST_CHECK_EQUAL(df, df2);
      df.assign(-c);
      df2 = -df2;
      BOOST_CHECK_EQUAL(df, df2);
#ifdef TEST_GMP
      typedef number<gmp_float<500> > mpf_type;
      mpf_type mpf(c);
      mpf_type mpf2(c.str());
      BOOST_CHECK_EQUAL(mpf, mpf2);
      mpf.assign(-c);
      mpf2 = -mpf2;
      BOOST_CHECK_EQUAL(mpf, mpf2);
#endif
#ifdef TEST_MPFR
      typedef number<mpfr_float_backend<500> > mpfr_type;
      mpfr_type mpfr(c);
      mpfr_type mpfr2(c.str());
      BOOST_CHECK_EQUAL(mpfr, mpfr2);
      mpfr.assign(-c);
      mpfr2 = -mpfr2;
      BOOST_CHECK_EQUAL(mpfr, mpfr2);
#endif
      //
      // Now float to float:
      //
      df.assign(c);
      df /= dec_float_500(gen());
      dec_float_500 tol("1e-500");
#ifdef TEST_GMP
      mpf.assign(df);
      mpf2 = static_cast<mpf_type>(df.str());
      BOOST_CHECK_EQUAL(mpf, mpf2);
      df.assign(mpf);
      df2 = static_cast<dec_float_500>(mpf.str());
      BOOST_CHECK(fabs((df - df2) / df) < tol);
#endif
#ifdef TEST_MPFR
      mpfr.assign(df);
      mpfr2 = static_cast<mpfr_type>(df.str());
      BOOST_CHECK_EQUAL(mpfr, mpfr2);
      df.assign(mpfr);
      df2 = static_cast<dec_float_500>(mpfr.str());
      BOOST_CHECK(fabs((df - df2) / df) < tol);
#endif
      //
      // Rational to rational conversions:
      //
      cpp_rational cppr(c, gen()), cppr2, cppr3;
#ifdef TEST_GMP
      mpq_rational mpq(cppr);
      cppr2.assign(mpq);
      BOOST_CHECK_EQUAL(cppr, cppr2);
#endif
#ifdef TEST_TOMMATH
      tom_rational tr(cppr);
      cppr3.assign(tr);
      BOOST_CHECK_EQUAL(cppr, cppr3);
#endif
      //
      // Integer to rational conversions:
      //
#ifdef TEST_GMP
      mpq.assign(c);
      mpq_rational mpq2 = static_cast<mpq_rational>(c.str());
      BOOST_CHECK_EQUAL(mpq, mpq2);
#endif
#ifdef TEST_TOMMATH
      tr.assign(c);
      tom_rational tr2 = static_cast<tom_rational>(c.str());
      BOOST_CHECK_EQUAL(tr, tr2);
#endif
      //
      // Rational to float:
      //
      df.assign(cppr);
      df2.assign(numerator(cppr));
      df2 /= dec_float_500(denominator(cppr));
      BOOST_CHECK(fabs(df - df2) / df2 < tol);
      //
      // Float to rational:
      //
      static const int max_range = std::numeric_limits<double>::digits >= std::numeric_limits<int>::digits ? std::numeric_limits<int>::max() : (1 << (std::numeric_limits<double>::digits - 1)) - 1;
      static const int min_range = std::numeric_limits<double>::digits >= std::numeric_limits<int>::digits ? std::numeric_limits<int>::min() : -(1 << (std::numeric_limits<double>::digits - 1)) + 1;
      static const boost::random::uniform_int_distribution<> i_val_dist(min_range, max_range);
      static const boost::random::uniform_int_distribution<> i_exp_dist(std::numeric_limits<double>::min_exponent, std::numeric_limits<double>::max_exponent - 2 - std::numeric_limits<int>::digits);
      int iv = i_val_dist(small_gen);
      int eval = i_exp_dist(small_gen);
      double dv = iv;
      dv = ldexp(dv, eval);
      cppr = dv;
      cppr2 = iv;
      cpp_int cppi = 1;
      cppi <<= abs(eval);
      if(eval < 0)
         cppr2 /= cppi;
      else
         cppr2 *= cppi;
      BOOST_CHECK_EQUAL(cppr, cppr2);
      //
      // Again but with bigger numbers:
      //
      cpp_int cppi2 = gen();
      number<cpp_bin_float<1030> > cppbf(cppi2);
      cppbf = ldexp(cppbf, eval);
      cppr.assign(cppbf);
      cppr2 = cppi2;
      if(eval < 0)
         cppr2 /= cppi;
      else
         cppr2 *= cppi;
      BOOST_CHECK_EQUAL(cppr, cppr2);
      //
      // MSVC will compile either the code above, or
      // the code below, but not both in the same file.
      // Other compilers including Intel in msvc-compatibity
      // mode have no such difficulty.  Indeed the fact that
      // the presence of the code below causes the code above to
      // fail to compile strongly suggests a compiler bug.
      //
#if !defined(BOOST_MSVC)
      //
      // Again but with bigger base 10 numbers:
      //
      number<cpp_dec_float<std::numeric_limits<int1024_t>::digits10 + 3> > cppdec2(cppi2);
      cppdec2 = scalbn(cppdec2, eval);
      cppr.assign(cppdec2);
      cppr2 = cppi2;
      cppi = 10;
      cppi = pow(cppi, abs(eval));
      if(eval < 0)
         cppr2 /= cppi;
      else
         cppr2 *= cppi;
      BOOST_CHECK_EQUAL(cppr, cppr2);
#endif
   }

   return boost::report_errors();
}



