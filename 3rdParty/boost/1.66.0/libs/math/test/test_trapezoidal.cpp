/*
 * Copyright Nick Thompson, 2017
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#define BOOST_TEST_MODULE trapezoidal_quadrature

#include <boost/test/included/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/math/concepts/real_concept.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/quadrature/trapezoidal.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using boost::multiprecision::cpp_bin_float_50;
using boost::multiprecision::cpp_bin_float_100;
using boost::math::quadrature::trapezoidal;

template<class Real>
void test_constant()
{
    std::cout << "Testing constants are integrated correctly by the adaptive trapezoidal routine on type " << boost::typeindex::type_id<Real>().pretty_name()  << "\n";

    auto f = [](Real)->Real { return boost::math::constants::half<Real>(); };
    Real Q = trapezoidal<decltype(f), Real>(f, (Real) 0.0, (Real) 10.0);
    BOOST_CHECK_CLOSE(Q, 5.0, 100*std::numeric_limits<Real>::epsilon());
}


template<class Real>
void test_rational_periodic()
{
    using boost::math::constants::two_pi;
    using boost::math::constants::third;
    std::cout << "Testing that rational periodic functions are integrated correctly by trapezoidal rule on type " << boost::typeindex::type_id<Real>().pretty_name() << "\n";

    auto f = [](Real x)->Real { return 1/(5 - 4*cos(x)); };

    Real tol = 100*boost::math::tools::epsilon<Real>();
    Real Q = trapezoidal(f, (Real) 0.0, two_pi<Real>(), tol);

    BOOST_CHECK_CLOSE_FRACTION(Q, two_pi<Real>()*third<Real>(), 10*tol);
}

template<class Real>
void test_bump_function()
{
    std::cout << "Testing that bump functions are integrated correctly by trapezoidal rule on type " << boost::typeindex::type_id<Real>().pretty_name() << "\n";
    auto f = [](Real x)->Real {
        if( x>= 1 || x <= -1)
        {
            return (Real) 0;
        }
        return (Real) exp(-(Real) 1/(1-x*x));
    };
    Real tol = boost::math::tools::epsilon<Real>();
    Real Q = trapezoidal(f, (Real) -1, (Real) 1, tol);
    // 2*NIntegrate[Exp[-(1/(1 - x^2))], {x, 0, 1}, WorkingPrecision -> 210]
    Real Q_exp = boost::lexical_cast<Real>("0.44399381616807943782304892117055266376120178904569749730748455394704");
    BOOST_CHECK_CLOSE_FRACTION(Q, Q_exp, 15*tol);
}

template<class Real>
void test_zero_function()
{
    std::cout << "Testing that zero functions are integrated correctly by trapezoidal rule on type " << boost::typeindex::type_id<Real>().pretty_name() << "\n";
    auto f = [](Real)->Real { return (Real) 0;};
    Real tol = 100* boost::math::tools::epsilon<Real>();
    Real Q = trapezoidal(f, (Real) -1, (Real) 1, tol);
    BOOST_CHECK_SMALL(Q, 100*tol);
}

template<class Real>
void test_sinsq()
{
    std::cout << "Testing that sin(x)^2 is integrated correctly by the trapezoidal rule on type " << boost::typeindex::type_id<Real>().pretty_name() << "\n";
    auto f = [](Real x)->Real { return sin(10*x)*sin(10*x); };
    Real tol = 100* boost::math::tools::epsilon<Real>();
    Real Q = trapezoidal(f, (Real) 0, (Real) boost::math::constants::pi<Real>(), tol);
    BOOST_CHECK_CLOSE_FRACTION(Q, boost::math::constants::half_pi<Real>(), tol);

}

template<class Real>
void test_slowly_converging()
{
    using std::sqrt;
    std::cout << "Testing that non-periodic functions are correctly integrated by the trapezoidal rule, even if slowly, on type " << boost::typeindex::type_id<Real>().pretty_name() << "\n";
    // This function is not periodic, so it should not be fast to converge:
    auto f = [](Real x)->Real { using std::sqrt;  return sqrt(1 - x*x); };

    Real tol = sqrt(sqrt(boost::math::tools::epsilon<Real>()));
    Real error_estimate;
    Real Q = trapezoidal(f, (Real) 0, (Real) 1, tol, 15, &error_estimate);
    BOOST_CHECK_CLOSE_FRACTION(Q, boost::math::constants::half_pi<Real>()/2, 10*tol);
}

template<class Real>
void test_rational_sin()
{
    using std::pow;
    using std::sin;
    using boost::math::constants::two_pi;
    using boost::math::constants::half;
    std::cout << "Testing that a rational sin function is integrated correctly by the trapezoidal rule on type " << boost::typeindex::type_id<Real>().pretty_name() << "\n";
    Real a = 5;
    auto f = [=](Real x)->Real { using std::sin;  Real t = a + sin(x); return 1.0f / (t*t); };

    Real expected = two_pi<Real>()*a/pow(a*a - 1, 3*half<Real>());
    Real tol = 100* boost::math::tools::epsilon<Real>();
    Real Q = trapezoidal(f, (Real) 0, (Real) boost::math::constants::two_pi<Real>(), tol);
    BOOST_CHECK_CLOSE_FRACTION(Q, expected, tol);
}

BOOST_AUTO_TEST_CASE(trapezoidal_quadrature)
{
    test_constant<float>();
    test_constant<double>();
    test_constant<long double>();
    test_constant<boost::math::concepts::real_concept>();
    test_constant<cpp_bin_float_50>();
    test_constant<cpp_bin_float_100>();

    test_rational_periodic<float>();
    test_rational_periodic<double>();
    test_rational_periodic<long double>();
    test_rational_periodic<boost::math::concepts::real_concept>();
    test_rational_periodic<cpp_bin_float_50>();
    test_rational_periodic<cpp_bin_float_100>();

    test_bump_function<float>();
    test_bump_function<double>();
    test_bump_function<long double>();
    test_rational_periodic<boost::math::concepts::real_concept>();
    test_rational_periodic<cpp_bin_float_50>();

    test_zero_function<float>();
    test_zero_function<double>();
    test_zero_function<long double>();
    test_zero_function<boost::math::concepts::real_concept>();
    test_zero_function<cpp_bin_float_50>();
    test_zero_function<cpp_bin_float_100>();

    test_sinsq<float>();
    test_sinsq<double>();
    test_sinsq<long double>();
    test_sinsq<boost::math::concepts::real_concept>();
    test_sinsq<cpp_bin_float_50>();
    test_sinsq<cpp_bin_float_100>();

    test_slowly_converging<float>();
    test_slowly_converging<double>();
    test_slowly_converging<long double>();
    test_slowly_converging<boost::math::concepts::real_concept>();

    test_rational_sin<float>();
    test_rational_sin<double>();
    test_rational_sin<long double>();
    test_rational_sin<boost::math::concepts::real_concept>();
    test_rational_sin<cpp_bin_float_50>();

}
