//  (C) Copyright Matt Borland 2021.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//  Constepxr implementation of abs (see c.math.abs secion 26.8.2 of the ISO standard)

#ifndef BOOST_MATH_CCMATH_ABS
#define BOOST_MATH_CCMATH_ABS

#include <cmath>
#include <type_traits>
#include <limits>
#include <boost/math/tools/is_constant_evaluated.hpp>
#include <boost/math/ccmath/isnan.hpp>
#include <boost/math/ccmath/isinf.hpp>

namespace boost::math::ccmath {

namespace detail {

template <typename T> 
inline constexpr T abs_impl(T x) noexcept
{
    return boost::math::ccmath::isnan(x) ? std::numeric_limits<T>::quiet_NaN() : 
           boost::math::ccmath::isinf(x) ? std::numeric_limits<T>::infinity() : 
           x == -0 ? T(0) :
           x == (std::numeric_limits<T>::min)() ? std::numeric_limits<T>::quiet_NaN() : 
           x > 0 ? x : -x;
}

} // Namespace detail

template <typename T, std::enable_if_t<!std::is_unsigned_v<T>, bool> = true>
inline constexpr T abs(T x) noexcept
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(x))
    {
        return detail::abs_impl<T>(x);
    }
    else
    {
        using std::abs;
        return abs(x);
    }
}

// If abs() is called with an argument of type X for which is_unsigned_v<X> is true and if X
// cannot be converted to int by integral promotion (7.3.7), the program is ill-formed.
template <typename T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
inline constexpr T abs(T x) noexcept
{
    if constexpr (std::is_convertible_v<T, int>)
    {
        return detail::abs_impl<int>(static_cast<int>(x));
    }
    else
    {
        static_assert(sizeof(T) == 0, "Taking the absolute value of an unsigned value not covertible to int is UB.");
        return T(0); // Unreachable, but suppresses warnings
    }
}

inline constexpr long int labs(long int j) noexcept
{
    return boost::math::ccmath::abs(j);
}

inline constexpr long long int llabs(long long int j) noexcept
{
    return boost::math::ccmath::abs(j);
}

} // Namespaces

#endif // BOOST_MATH_CCMATH_ABS
