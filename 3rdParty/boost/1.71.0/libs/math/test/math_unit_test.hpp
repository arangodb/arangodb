// Copyright Nick Thompson, 2019
// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_MATH_TEST_TEST_HPP
#define BOOST_MATH_TEST_TEST_HPP
#include <atomic>
#include <iostream>
#include <iomanip>
#include <cmath> // for std::isnan
#include <boost/assert.hpp>
#include <boost/math/special_functions/next.hpp>
#include <boost/core/demangle.hpp>


namespace boost { namespace math { namespace  test {

namespace detail {
    static std::atomic<int64_t> global_error_count{0};
    static std::atomic<int64_t> total_ulp_distance{0};
}

template<class Real>
bool check_mollified_close(Real expected, Real computed, Real tol, std::string const & filename, std::string const & function, int line)
{
    using std::isnan;
    BOOST_ASSERT_MSG(!isnan(tol), "Tolerance cannot be a nan.");
    BOOST_ASSERT_MSG(!isnan(expected), "Expected value cannot be a nan.");
    BOOST_ASSERT_MSG(tol >= 0, "Tolerance must be non-negative.");
    if (isnan(computed)) {
        std::ios_base::fmtflags f( std::cerr.flags() );
        std::cerr << std::setprecision(3);
        std::cerr << "\033[0;31mError at " << filename << ":" << function << ":" << line << ":\n"
                  << " \033[0m Computed value is a nan\n";
        std::cerr.flags(f);
        ++detail::global_error_count;
        return false;
    }
    using std::max;
    using std::abs;
    Real denom = max(abs(expected), Real(1));
    Real mollified_relative_error = abs(expected - computed)/denom;
    if (mollified_relative_error > tol)
    {
        Real dist = abs(boost::math::float_distance(expected, computed));
        detail::total_ulp_distance += static_cast<int64_t>(dist);
        std::ios_base::fmtflags f( std::cerr.flags() );
        std::cerr << std::setprecision(3);
        std::cerr << "\033[0;31mError at " << filename << ":" << function << ":" << line << ":\n"
                  << " \033[0m Mollified relative error in " << boost::core::demangle(typeid(Real).name())<< " precision is " << mollified_relative_error
                  << ", which exceeds " << tol << ", error/tol  = " << mollified_relative_error/tol << ".\n"
                  << std::setprecision(std::numeric_limits<Real>::digits10) << std::showpos
                  << "  Expected: " << std::defaultfloat << std::fixed << expected << std::hexfloat << " = " << expected << "\n"
                  << "  Computed: " << std::defaultfloat << std::fixed << computed << std::hexfloat << " = " << computed << "\n"
                  << std::defaultfloat
                  << "  ULP distance: " << dist << "\n";
        std::cerr.flags(f);
        ++detail::global_error_count;

        return false;
    }
    return true;
}

template<class PreciseReal, class Real>
bool check_ulp_close(PreciseReal expected1, Real computed, size_t ulps, std::string const & filename, std::string const & function, int line)
{
    using std::max;
    using std::abs;
    using std::isnan;
    BOOST_ASSERT_MSG(sizeof(PreciseReal) >= sizeof(Real),
                     "The expected number must be computed in higher (or equal) precision than the number being tested.");

    BOOST_ASSERT_MSG(!isnan(expected1), "Expected value cannot be a nan.");
    if (isnan(computed))
    {
        std::ios_base::fmtflags f( std::cerr.flags() );
        std::cerr << std::setprecision(3);
        std::cerr << "\033[0;31mError at " << filename << ":" << function << ":" << line << ":\n"
                  << " \033[0m Computed value is a nan\n";
        std::cerr.flags(f);
        ++detail::global_error_count;
        return false;
    }

    Real expected = Real(expected1);
    Real dist = abs(boost::math::float_distance(expected, computed));
    if (dist > ulps)
    {
        detail::total_ulp_distance += static_cast<int64_t>(dist);
        Real denom = max(abs(expected), Real(1));
        Real mollified_relative_error = abs(expected - computed)/denom;
        std::ios_base::fmtflags f( std::cerr.flags() );
        std::cerr << std::setprecision(3);
        std::cerr << "\033[0;31mError at " << filename << ":" << function << ":" << line << ":\n"
                  << " \033[0m ULP distance in " << boost::core::demangle(typeid(Real).name())<< " precision is " << dist
                  << ", which exceeds " << ulps;
                  if (ulps > 0)
                  {
                      std::cerr << ", error/ulps  = " << dist/static_cast<Real>(ulps) << ".\n";
                  }
                  else
                  {
                      std::cerr << ".\n";
                  }
        std::cerr << std::setprecision(std::numeric_limits<Real>::digits10) << std::showpos
                  << "  Expected: " << std::defaultfloat << std::fixed << expected << std::hexfloat << " = " << expected << "\n"
                  << "  Computed: " << std::defaultfloat << std::fixed << computed << std::hexfloat << " = " << computed << "\n"
                  << std::defaultfloat
                  << "  Mollified relative error: " << mollified_relative_error << "\n";
        std::cerr.flags(f);
        ++detail::global_error_count;
        return false;
    }
    return true;
}


int report_errors()
{
    if (detail::global_error_count > 0)
    {
        std::cerr << "\033[0;31mError count: " << detail::global_error_count;
        if (detail::total_ulp_distance > 0) {
            std::cerr << ", total ulp distance = " << detail::total_ulp_distance << "\n";
        }
        else {
            // else we overflowed the ULPs counter and all we could print is a bizarre negative number.
            std::cerr << "\n";
        }

        detail::global_error_count = 0;
        detail::total_ulp_distance = 0;
        return 1;
    }
    std::cout << "\x1B[32mNo errors detected.\n";
    return 0;
}

}}}

#define CHECK_MOLLIFIED_CLOSE(X, Y, Z) boost::math::test::check_mollified_close< typename std::remove_reference<decltype((Y))>::type>((X), (Y), (Z), __FILE__, __func__, __LINE__)

#define CHECK_ULP_CLOSE(X, Y, Z) boost::math::test::check_ulp_close((X), (Y), (Z), __FILE__, __func__, __LINE__)

#endif
