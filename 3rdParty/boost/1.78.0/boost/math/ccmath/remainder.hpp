//  (C) Copyright Matt Borland 2021.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MATH_CCMATH_REMAINDER_HPP
#define BOOST_MATH_CCMATH_REMAINDER_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <boost/math/tools/is_constant_evaluated.hpp>
#include <boost/math/ccmath/abs.hpp>
#include <boost/math/ccmath/isinf.hpp>
#include <boost/math/ccmath/isnan.hpp>
#include <boost/math/ccmath/isfinite.hpp>
#include <boost/math/ccmath/modf.hpp>

namespace boost::math::ccmath {

namespace detail {

template <typename T>
inline constexpr T remainder_impl(const T x, const T y) noexcept
{
    T n = 0;
    const T fractional_part = boost::math::ccmath::modf((x / y), &n);

    if(fractional_part > T(1.0/2))
    {
        ++n;
    }
    else if(fractional_part < T(-1.0/2))
    {
        --n;
    }

    return x - n*y;
}

} // Namespace detail

template <typename Real, std::enable_if_t<!std::is_integral_v<Real>, bool> = true>
inline constexpr Real remainder(Real x, Real y) noexcept
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(x))
    {
        return boost::math::ccmath::isinf(x) && !boost::math::ccmath::isnan(y) ? std::numeric_limits<Real>::quiet_NaN() :
               boost::math::ccmath::abs(y) == Real(0) && !boost::math::ccmath::isnan(x) ? std::numeric_limits<Real>::quiet_NaN() :
               boost::math::ccmath::isnan(x) || boost::math::ccmath::isnan(y) ? std::numeric_limits<Real>::quiet_NaN() :
               boost::math::ccmath::detail::remainder_impl<Real>(x, y);
    }
    else
    {
        using std::remainder;
        return remainder(x, y);
    }
}

template <typename T1, typename T2>
inline constexpr auto remainder(T1 x, T2 y) noexcept
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(x))
    {
        // If the type is an integer (e.g. epsilon == 0) then set the epsilon value to 1 so that type is at a minimum 
        // cast to double
        constexpr auto T1p = std::numeric_limits<T1>::epsilon() > 0 ? std::numeric_limits<T1>::epsilon() : 1;
        constexpr auto T2p = std::numeric_limits<T2>::epsilon() > 0 ? std::numeric_limits<T2>::epsilon() : 1;
        
        using promoted_type = 
                              #ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
                              std::conditional_t<T1p <= LDBL_EPSILON && T1p <= T2p, T1,
                              std::conditional_t<T2p <= LDBL_EPSILON && T2p <= T1p, T2,
                              #endif
                              std::conditional_t<T1p <= DBL_EPSILON && T1p <= T2p, T1,
                              std::conditional_t<T2p <= DBL_EPSILON && T2p <= T1p, T2, double
                              #ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
                              >>>>;
                              #else
                              >>;
                              #endif

        return boost::math::ccmath::remainder(promoted_type(x), promoted_type(y));
    }
    else
    {
        using std::remainder;
        return remainder(x, y);
    }
}

inline constexpr float remainderf(float x, float y) noexcept
{
    return boost::math::ccmath::remainder(x, y);
}

#ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
inline constexpr long double remainderl(long double x, long double y) noexcept
{
    return boost::math::ccmath::remainder(x, y);
}
#endif

} // Namespaces

#endif // BOOST_MATH_CCMATH_REMAINDER_HPP
