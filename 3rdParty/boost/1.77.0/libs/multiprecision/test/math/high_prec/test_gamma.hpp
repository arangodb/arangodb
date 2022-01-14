// Copyright John Maddock 2006.
// Copyright Paul A. Bristow 2007, 2009
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_MATH_OVERFLOW_ERROR_POLICY ignore_error

#include <boost/math/special_functions/gamma.hpp>
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>
#include <boost/math/tools/stats.hpp>
#include <boost/math/tools/test.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/array.hpp>
#include "libs/math/test/functor.hpp"

#include "libs/math/test/handle_test_result.hpp"
#include "../table_type.hpp"

#ifndef SC_
#define SC_(x) static_cast<typename table_type<T>::type>(BOOST_JOIN(x, L))
#endif

template <class Real, class T>
void do_test_gamma(const T& data, const char* type_name, const char* test_name)
{
   typedef typename T::value_type row_type;
   typedef Real                   value_type;

   typedef value_type (*pg)(value_type);
#if defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
   pg funcp = boost::math::tgamma<value_type>;
#else
   pg funcp = boost::math::tgamma;
#endif

   boost::math::tools::test_result<value_type> result;

   std::cout << "Testing " << test_name << " with type " << type_name
             << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

   //
   // test tgamma against data:
   //
   result = boost::math::tools::test_hetero<Real>(
       data,
       bind_func<Real>(funcp, 0),
       extract_result<Real>(1));
   handle_test_result(result, data[result.worst()], result.worst(), type_name, "boost::math::tgamma", test_name);

   //
   // test lgamma against data:
   //
#if defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
   funcp = boost::math::lgamma<value_type>;
#else
   funcp    = boost::math::lgamma;
#endif
   result = boost::math::tools::test_hetero<Real>(
       data,
       bind_func<Real>(funcp, 0),
       extract_result<Real>(2));
   handle_test_result(result, data[result.worst()], result.worst(), type_name, "boost::math::lgamma", test_name);

   std::cout << std::endl;
}

template <class T>
void test_gamma(T, const char* name)
{
#include "gamma.ipp"

   do_test_gamma<T>(gamma, name, "random values");

#include "gamma_1_2.ipp"

   do_test_gamma<T>(gamma_1_2, name, "Values near 1 and 2");

#include "gamma_0.ipp"

   do_test_gamma<T>(gamma_0, name, "Values near 0");

#include "gamma_neg.ipp"

   do_test_gamma<T>(gamma_neg, name, "Negative arguments");
}
