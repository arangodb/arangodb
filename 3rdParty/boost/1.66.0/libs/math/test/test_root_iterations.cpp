//  (C) Copyright John Maddock 2015.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <pch.hpp>

#ifndef BOOST_NO_CXX11_HDR_TUPLE

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/math/tools/roots.hpp>
#include <boost/test/results_collector.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/math/special_functions/cbrt.hpp>
#include <iostream>
#include <iomanip>
#include <tuple>

// No derivatives - using TOMS748 internally.
struct cbrt_functor_noderiv
{ //  cube root of x using only function - no derivatives.
   cbrt_functor_noderiv(double to_find_root_of) : a(to_find_root_of)
   { // Constructor just stores value a to find root of.
   }
   double operator()(double x)
   {
      double fx = x*x*x - a; // Difference (estimate x^3 - a).
      return fx;
   }
private:
   double a; // to be 'cube_rooted'.
}; // template <class T> struct cbrt_functor_noderiv

// Using 1st derivative only Newton-Raphson
struct cbrt_functor_deriv
{ // Functor also returning 1st derviative.
   cbrt_functor_deriv(double const& to_find_root_of) : a(to_find_root_of)
   { // Constructor stores value a to find root of,
      // for example: calling cbrt_functor_deriv<double>(x) to use to get cube root of x.
   }
   std::pair<double, double> operator()(double const& x)
   { // Return both f(x) and f'(x).
      double fx = x*x*x - a; // Difference (estimate x^3 - value).
      double dx = 3 * x*x; // 1st derivative = 3x^2.
      return std::make_pair(fx, dx); // 'return' both fx and dx.
   }
private:
   double a; // to be 'cube_rooted'.
};
// Using 1st and 2nd derivatives with Halley algorithm.
struct cbrt_functor_2deriv
{ // Functor returning both 1st and 2nd derivatives.
   cbrt_functor_2deriv(double const& to_find_root_of) : a(to_find_root_of)
   { // Constructor stores value a to find root of, for example:
      // calling cbrt_functor_2deriv<double>(x) to get cube root of x,
   }
   std::tuple<double, double, double> operator()(double const& x)
   { // Return both f(x) and f'(x) and f''(x).
      double fx = x*x*x - a; // Difference (estimate x^3 - value).
      double dx = 3 * x*x; // 1st derivative = 3x^2.
      double d2x = 6 * x; // 2nd derivative = 6x.
      return std::make_tuple(fx, dx, d2x); // 'return' fx, dx and d2x.
   }
private:
   double a; // to be 'cube_rooted'.
};

BOOST_AUTO_TEST_CASE( test_main )
{
   int newton_limits = static_cast<int>(std::numeric_limits<double>::digits * 0.6);

   double arg = 1e-50;
   while(arg < 1e50)
   {
      double result = boost::math::cbrt(arg);
      //
      // Start with a really bad guess 5 times below the result:
      //
      double guess = result / 5;
      boost::uintmax_t iters = 1000;
      // TOMS algo first:
      std::pair<double, double> r = boost::math::tools::bracket_and_solve_root(cbrt_functor_noderiv(arg), guess, 2.0, true, boost::math::tools::eps_tolerance<double>(), iters);
      BOOST_CHECK_CLOSE_FRACTION((r.first + r.second) / 2, result, std::numeric_limits<double>::epsilon() * 4);
      BOOST_CHECK_LE(iters, 14);
      // Newton next:
      iters = 1000;
      double dr = boost::math::tools::newton_raphson_iterate(cbrt_functor_deriv(arg), guess, guess / 2, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 12);
      // Halley next:
      iters = 1000;
      dr = boost::math::tools::halley_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 7);
      // Schroder next:
      iters = 1000;
      dr = boost::math::tools::schroder_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 11);
      //
      // Over again with a bad guess 5 times larger than the result:
      //
      iters = 1000;
      guess = result * 5;
      r = boost::math::tools::bracket_and_solve_root(cbrt_functor_noderiv(arg), guess, 2.0, true, boost::math::tools::eps_tolerance<double>(), iters);
      BOOST_CHECK_CLOSE_FRACTION((r.first + r.second) / 2, result, std::numeric_limits<double>::epsilon() * 4);
      BOOST_CHECK_LE(iters, 14);
      // Newton next:
      iters = 1000;
      dr = boost::math::tools::newton_raphson_iterate(cbrt_functor_deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 12);
      // Halley next:
      iters = 1000;
      dr = boost::math::tools::halley_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 7);
      // Schroder next:
      iters = 1000;
      dr = boost::math::tools::schroder_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 11);
      //
      // A much better guess, 1% below result:
      //
      iters = 1000;
      guess = result * 0.9;
      r = boost::math::tools::bracket_and_solve_root(cbrt_functor_noderiv(arg), guess, 2.0, true, boost::math::tools::eps_tolerance<double>(), iters);
      BOOST_CHECK_CLOSE_FRACTION((r.first + r.second) / 2, result, std::numeric_limits<double>::epsilon() * 4);
      BOOST_CHECK_LE(iters, 12);
      // Newton next:
      iters = 1000;
      dr = boost::math::tools::newton_raphson_iterate(cbrt_functor_deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 5);
      // Halley next:
      iters = 1000;
      dr = boost::math::tools::halley_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 3);
      // Schroder next:
      iters = 1000;
      dr = boost::math::tools::schroder_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 4);
      //
      // A much better guess, 1% above result:
      //
      iters = 1000;
      guess = result * 1.1;
      r = boost::math::tools::bracket_and_solve_root(cbrt_functor_noderiv(arg), guess, 2.0, true, boost::math::tools::eps_tolerance<double>(), iters);
      BOOST_CHECK_CLOSE_FRACTION((r.first + r.second) / 2, result, std::numeric_limits<double>::epsilon() * 4);
      BOOST_CHECK_LE(iters, 12);
      // Newton next:
      iters = 1000;
      dr = boost::math::tools::newton_raphson_iterate(cbrt_functor_deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 5);
      // Halley next:
      iters = 1000;
      dr = boost::math::tools::halley_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 3);
      // Schroder next:
      iters = 1000;
      dr = boost::math::tools::schroder_iterate(cbrt_functor_2deriv(arg), guess, result / 10, result * 10, newton_limits, iters);
      BOOST_CHECK_CLOSE_FRACTION(dr, result, std::numeric_limits<double>::epsilon() * 2);
      BOOST_CHECK_LE(iters, 4);

      arg *= 3.5;
   }
}

#else

int main() { return 0; }

#endif
