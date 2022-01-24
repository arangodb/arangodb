//  (C) Copyright Matt Borland 2021.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//  Constexpr implementation of sqrt function

#ifndef BOOST_MATH_CCMATH_SQRT
#define BOOST_MATH_CCMATH_SQRT

#include <cmath>
#include <limits>
#include <type_traits>
#include <boost/math/ccmath/isnan.hpp>
#include <boost/math/ccmath/isinf.hpp>
#include <boost/math/tools/is_constant_evaluated.hpp>

namespace boost::math::ccmath { 

namespace detail {

template <typename Real>
inline constexpr Real sqrt_impl_2(Real x, Real s, Real s2)
{
    return !(s < s2) ? s2 : sqrt_impl_2(x, (x / s + s) / 2, s);
}

template <typename Real>
inline constexpr Real sqrt_impl_1(Real x, Real s)
{
    return sqrt_impl_2(x, (x / s + s) / 2, s);
}

template <typename Real>
inline constexpr Real sqrt_impl(Real x)
{
    return sqrt_impl_1(x, x > 1 ? x : Real(1));
}

} // namespace detail

template <typename Real, std::enable_if_t<!std::is_integral_v<Real>, bool> = true>
inline constexpr Real sqrt(Real x)
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(x))
    {
        return boost::math::ccmath::isnan(x) ? std::numeric_limits<Real>::quiet_NaN() : 
               boost::math::ccmath::isinf(x) ? std::numeric_limits<Real>::infinity() : 
               detail::sqrt_impl<Real>(x);
    }
    else
    {
        using std::sqrt;
        return sqrt(x);
    }
}

template <typename Z, std::enable_if_t<std::is_integral_v<Z>, bool> = true>
inline constexpr double sqrt(Z x)
{
    return detail::sqrt_impl<double>(static_cast<double>(x));
}

} // Namespaces

#endif // BOOST_MATH_CCMATH_SQRT
