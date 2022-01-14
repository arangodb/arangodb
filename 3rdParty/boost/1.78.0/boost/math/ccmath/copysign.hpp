//  (C) Copyright Matt Borland 2021.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MATH_CCMATH_COPYSIGN_HPP
#define BOOST_MATH_CCMATH_COPYSIGN_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <boost/math/tools/is_constant_evaluated.hpp>
#include <boost/math/ccmath/abs.hpp>

namespace boost::math::ccmath {

namespace detail {

template <typename T>
inline constexpr T copysign_impl(const T mag, const T sgn) noexcept
{
    if(sgn >= 0)
    {
        return boost::math::ccmath::abs(mag);
    }
    else
    {
        return -boost::math::ccmath::abs(mag);
    }
}

} // Namespace detail

template <typename Real, std::enable_if_t<!std::is_integral_v<Real>, bool> = true>
inline constexpr Real copysign(Real mag, Real sgn) noexcept
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(mag))
    {
        return boost::math::ccmath::detail::copysign_impl(mag, sgn);
    }
    else
    {
        using std::copysign;
        return copysign(mag, sgn);
    }
}

template <typename T1, typename T2>
inline constexpr auto copysign(T1 mag, T2 sgn) noexcept
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(mag))
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

        return boost::math::ccmath::copysign(promoted_type(mag), promoted_type(sgn));
    }
    else
    {
        using std::copysign;
        return copysign(mag, sgn);
    }
}

inline constexpr float copysignf(float mag, float sgn) noexcept
{
    return boost::math::ccmath::copysign(mag, sgn);
}

#ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
inline constexpr long double copysignl(long double mag, long double sgn) noexcept
{
    return boost::math::ccmath::copysign(mag, sgn);
}
#endif

} // Namespaces

#endif // BOOST_MATH_CCMATH_COPYSIGN_HPP
