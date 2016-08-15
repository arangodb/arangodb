// Copyright John Maddock 2006.
// Copyright Paul A. Bristow 2007, 2009
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/math/concepts/real_concept.hpp>
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <boost/math/tools/stats.hpp>
#include <boost/math/tools/test.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <boost/array.hpp>
#include "functor.hpp"

#include "handle_test_result.hpp"
#include "table_type.hpp"

#ifndef SC_
#define SC_(x) static_cast<typename table_type<T>::type>(BOOST_JOIN(x, L))
#endif

template <class T>
T ibeta_forwarder(T a, T b, T x)
{
   T derivative;
   boost::math::detail::ibeta_imp(a, b, x, boost::math::policies::policy<>(), false, true, &derivative);
   return derivative;
}

template <class Real, class T>
void do_test_beta(const T& data, const char* type_name, const char* test_name)
{
   typedef Real                   value_type;

   typedef value_type (*pg)(value_type, value_type, value_type);
#if defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
   pg funcp = boost::math::ibeta_derivative<value_type, value_type, value_type>;
#else
   pg funcp = boost::math::ibeta_derivative;
#endif

   boost::math::tools::test_result<value_type> result;

#if !(defined(ERROR_REPORTING_MODE) && !defined(BETA_INC_FUNCTION_TO_TEST))
   std::cout << "Testing " << test_name << " with type " << type_name
      << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

   //
   // test ibeta_derivative against data:
   //
   result = boost::math::tools::test_hetero<Real>(
      data,
      bind_func<Real>(funcp, 0, 1, 2),
      extract_result<Real>(3));
   handle_test_result(result, data[result.worst()], result.worst(), type_name, "beta (incomplete)", test_name);
#endif

#if defined(BOOST_MATH_NO_DEDUCED_FUNCTION_POINTERS)
   funcp = ibeta_forwarder<value_type>;
#else
   funcp = ibeta_forwarder;
#endif

   if(boost::math::tools::digits<value_type>() > 40)
   {
      //
      // test ibeta_derivative against data:
      //
      result = boost::math::tools::test_hetero<Real>(
         data,
         bind_func<Real>(funcp, 0, 1, 2),
         extract_result<Real>(3));
      handle_test_result(result, data[result.worst()], result.worst(), type_name, "beta (incomplete, internal call test)", test_name);
   }
}

template <class T>
void test_beta(T, const char* name)
{
   //
   // The actual test data is rather verbose, so it's in a separate file
   //
   // The contents are as follows, each row of data contains
   // five items, input value a, input value b, integration limits x, beta(a, b, x) and ibeta(a, b, x):
   //
#if !defined(TEST_DATA) || (TEST_DATA == 1)
#  include "ibeta_derivative_small_data.ipp"

   do_test_beta<T>(ibeta_derivative_small_data, name, "Incomplete Beta Function Derivative: Small Values");
#endif

#if !defined(TEST_DATA) || (TEST_DATA == 2)
#  include "ibeta_derivative_data.ipp"

   do_test_beta<T>(ibeta_derivative_data, name, "Incomplete Beta Function Derivative: Medium Values");

#endif
#if !defined(TEST_DATA) || (TEST_DATA == 3)
#  include "ibeta_derivative_large_data.ipp"

   do_test_beta<T>(ibeta_derivative_large_data, name, "Incomplete Beta Function Derivative: Large and Diverse Values");
#endif

#if !defined(TEST_DATA) || (TEST_DATA == 4)
#  include "ibeta_derivative_int_data.ipp"

   do_test_beta<T>(ibeta_derivative_int_data, name, "Incomplete Beta Function Derivative: Small Integer Values");
#endif
}

template <class T>
void test_spots(T)
{
   using std::ldexp;
   T tolerance = boost::math::tools::epsilon<T>() * 40000;
      BOOST_CHECK_CLOSE(
         ::boost::math::ibeta_derivative(
            static_cast<T>(2),
            static_cast<T>(4),
            ldexp(static_cast<T>(1), -557)),
         static_cast<T>(4.23957586190238472641508753637420672781472122471791800210e-167L), tolerance * 4);
      BOOST_CHECK_CLOSE(
         ::boost::math::ibeta_derivative(
            static_cast<T>(2),
            static_cast<T>(4.5),
            ldexp(static_cast<T>(1), -557)),
         static_cast<T>(5.24647512910420109893867082626308082567071751558842352760e-167L), tolerance * 4);
}

