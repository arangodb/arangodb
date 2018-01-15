//  Copyright 2013 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

#ifndef BOOST_MP_MATH_SETUP_HPP
#define BOOST_MP_MATH_SETUP_HPP

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#define BOOST_MATH_OVERFLOW_ERROR_POLICY ignore_error
#undef BOOST_MATH_SMALL_CONSTANT
#define BOOST_MATH_SMALL_CONSTANT(x) x

#if !defined(TEST_MPF_50) && !defined(TEST_BACKEND) && !defined(TEST_CPP_DEC_FLOAT) \
      && !defined(TEST_MPFR_50) && !defined(TEST_FLOAT128) && !defined(TEST_CPP_BIN_FLOAT)
#  define TEST_MPF_50
#  define TEST_MPFR_50
#  define TEST_CPP_DEC_FLOAT
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
#include <boost/multiprecision/debug_adaptor.hpp>

#define MPF_TESTS    /*test(number<gmp_float<18> >(), "number<gmp_float<18> >");*/\
   mpf_float::default_precision(20);\
   test(mpf_float(), "number<gmp_float<0> > (20 digit precision)");\
   mpf_float::default_precision(35);\
   test(mpf_float(), "number<gmp_float<0> > (35 digit precision)");\
   test(number<gmp_float<30> >(), "number<gmp_float<30> >");\
   test(number<gmp_float<35> >(), "number<gmp_float<35> >");\
   /* there should be at least one test with expression templates off: */ \
   test(number<gmp_float<35>, et_off>(), "number<gmp_float<35>, et_off>");
#define MPF_SMALL_TESTS    /*test(number<gmp_float<18> >(), "number<gmp_float<18> >");*/\
   test(number<gmp_float<30> >(), "number<gmp_float<30> >");\
   test(number<gmp_float<35> >(), "number<gmp_float<35> >");\
   /* there should be at least one test with expression templates off: */ \
   test(number<gmp_float<35>, et_off>(), "number<gmp_float<35>, et_off>");\
   mpf_float::default_precision(20); \
   test(mpf_float(), "number<gmp_float<0> > (20 digit precision)"); \
   mpf_float::default_precision(35); \
   test(mpf_float(), "number<gmp_float<0> > (35 digit precision)"); \

typedef boost::multiprecision::number<boost::multiprecision::gmp_float<18> > test_type_1;
typedef boost::multiprecision::number<boost::multiprecision::gmp_float<30> > test_type_2;
typedef boost::multiprecision::number<boost::multiprecision::gmp_float<35> > test_type_3;
typedef boost::multiprecision::number<boost::multiprecision::gmp_float<35>, boost::multiprecision::et_off> test_type_4;
typedef boost::multiprecision::mpf_float test_type_5;

#else

#define MPF_TESTS
#define MPF_SMALL_TESTS

#endif

#if defined(TEST_MPFR_50)
#include <boost/multiprecision/mpfr.hpp>

#define MPFR_TESTS    test(number<mpfr_float_backend<18> >(), "number<mpfr_float_backend<18> >");\
   test(number<mpfr_float_backend<30> >(), "number<mpfr_float_backend<30> >");\
   test(number<mpfr_float_backend<35> >(), "number<mpfr_float_backend<35> >");\
  /* Test variable precision at 2 different precisions - checks our ability to handle dynamic changes in precision */\
  mpfr_float::default_precision(20);\
  test(mpfr_float(), "number<mpfr_float_backend<0> > (20-digit precision)");\
  mpfr_float::default_precision(35);\
  test(mpfr_float(), "number<mpfr_float_backend<0> > (35-digit precision)");

typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<18> > test_type_1;
typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<30> > test_type_2;
typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<35> > test_type_3;
typedef boost::multiprecision::mpfr_float test_type_4;

#else

#define MPFR_TESTS

#endif

#ifdef TEST_BACKEND
#include <boost/multiprecision/concepts/mp_number_archetypes.hpp>
#endif
#ifdef TEST_CPP_DEC_FLOAT
#include <boost/multiprecision/cpp_dec_float.hpp>

#define CPP_DEC_FLOAT_TESTS    test(number<cpp_dec_float<18> >(), "number<cpp_dec_float<18> >");\
   test(number<cpp_dec_float<30> >(), "number<cpp_dec_float<30> >");\
   test(number<cpp_dec_float<35, long long, std::allocator<void> > >(), "number<cpp_dec_float<35, long long, std::allocator<void> > >");

typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<18> > test_type_1;
typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<30> > test_type_2;
typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<35, long long, std::allocator<void> > > test_type_3;

#else

#define CPP_DEC_FLOAT_TESTS

#endif

#ifdef TEST_CPP_BIN_FLOAT
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/debug_adaptor.hpp>

//#define CPP_BIN_FLOAT_TESTS test(number<debug_adaptor<cpp_bin_float_quad::backend_type>, et_off>(), "cpp_bin_float_quad");
#define CPP_BIN_FLOAT_TESTS test(cpp_bin_float_quad(), "cpp_bin_float_quad");

//typedef boost::multiprecision::number<boost::multiprecision::debug_adaptor<boost::multiprecision::cpp_bin_float_quad::backend_type>, boost::multiprecision::et_off> test_type_1;
typedef boost::multiprecision::cpp_bin_float_quad test_type_1;

#else

#define CPP_BIN_FLOAT_TESTS

#endif

#ifdef TEST_FLOAT128
#include <boost/multiprecision/float128.hpp>

#define FLOAT128_TESTS test(float128(), "float128");

typedef boost::multiprecision::float128 test_type_1;

#else

#define FLOAT128_TESTS

#endif


#ifndef BOOST_MATH_TEST_TYPE
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#endif

#define ALL_TESTS \
 MPF_TESTS\
 MPFR_TESTS\
 CPP_DEC_FLOAT_TESTS\
 FLOAT128_TESTS\
 CPP_BIN_FLOAT_TESTS

#define ALL_SMALL_TESTS\
 MPF_SMALL_TESTS\
 MPFR_TESTS\
 CPP_DEC_FLOAT_TESTS\
 FLOAT128_TESTS\
 CPP_BIN_FLOAT_TESTS

#endif

