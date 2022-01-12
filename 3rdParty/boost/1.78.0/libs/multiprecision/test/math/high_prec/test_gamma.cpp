//  (C) Copyright John Maddock 2006, 2021.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test_gamma.hpp"
#ifdef TEST_MPFR
#include <boost/multiprecision/mpfr.hpp>
#endif
#ifdef TEST_MPF
#include <boost/multiprecision/gmp.hpp>
#endif
#ifdef TEST_CPP_BIN_FLOAT
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif
#ifdef TEST_CPP_DEC_FLOAT
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif

void expected_results()
{
   //
   // Define the max and mean errors expected for
   // various compilers and platforms.
   //
   add_expected_result(
       ".*",                                          // compiler
       ".*",                                          // stdlib
       ".*",                                          // platform
       ".*",                                          // test type(s)
       ".*near 1.*",                                  // test data group
       ".*lgamma.*", 100000000000LL, 100000000000LL); // test function
   add_expected_result(
       ".*",                          // compiler
       ".*",                          // stdlib
       ".*",                          // platform
       ".*",                          // test type(s)
       ".*near 0.*",                  // test data group
       ".*lgamma.*", 300000, 100000); // test function
   add_expected_result(
       ".*",                 // compiler
       ".*",                 // stdlib
       ".*",                 // platform
       ".*",                 // test type(s)
       ".*",                 // test data group
       ".*", 110000, 50000); // test function

   //
   // Finish off by printing out the compiler/stdlib/platform names,
   // we do this to make it easier to mark up expected error rates.
   //
   std::cout << "Tests run with " << BOOST_COMPILER << ", "
             << BOOST_STDLIB << ", " << BOOST_PLATFORM << std::endl;
}

#ifndef TEST_PRECISION
#define TEST_PRECISION 450
#endif

BOOST_AUTO_TEST_CASE(test_main)
{
   expected_results();
   BOOST_MATH_CONTROL_FP;

#ifdef TEST_MPFR
   typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<TEST_PRECISION> > mp_type;
   const char*                                                                            name = "number<mpfr_float_backend<" BOOST_STRINGIZE(TEST_PRECISION) "> >";
#endif
#ifdef TEST_MPF
   typedef boost::multiprecision::number<boost::multiprecision::gmp_float<TEST_PRECISION> > mp_type;
   const char*                                                                              name = "number<gmp_float<" BOOST_STRINGIZE(TEST_PRECISION) "> >";
#endif
#ifdef TEST_CPP_BIN_FLOAT
   typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<TEST_PRECISION> > mp_type;
   const char*                                                                                  name = "number<cpp_bin_float<" BOOST_STRINGIZE(TEST_PRECISION) "> >";
#endif
#ifdef TEST_CPP_DEC_FLOAT
   typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<TEST_PRECISION> > mp_type;
   const char*                                                                                  name = "number<cpp_dec_float<" BOOST_STRINGIZE(TEST_PRECISION) "> >";
#endif

   test_gamma(mp_type(0), name);
}
