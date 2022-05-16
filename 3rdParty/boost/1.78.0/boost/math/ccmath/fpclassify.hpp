//  (C) Copyright Matt Borland 2021.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MATH_CCMATH_FPCLASSIFY
#define BOOST_MATH_CCMATH_FPCLASSIFY

#include <cmath>
#include <limits>
#include <type_traits>
#include <boost/math/tools/is_constant_evaluated.hpp>
#include <boost/math/ccmath/abs.hpp>
#include <boost/math/ccmath/isinf.hpp>
#include <boost/math/ccmath/isnan.hpp>
#include <boost/math/ccmath/isfinite.hpp>

namespace boost::math::ccmath {

template <typename T, std::enable_if_t<!std::is_integral_v<T>, bool> = true>
inline constexpr int fpclassify(T x)
{
    if(BOOST_MATH_IS_CONSTANT_EVALUATED(x))
    {
        return boost::math::ccmath::isnan(x) ? FP_NAN :
               boost::math::ccmath::isinf(x) ? FP_INFINITE :
               boost::math::ccmath::abs(x) == T(0) ? FP_ZERO :
               boost::math::ccmath::abs(x) > 0 && boost::math::ccmath::abs(x) < (std::numeric_limits<T>::min)() ? FP_SUBNORMAL : FP_NORMAL;
    }
    else
    {
        using std::fpclassify;
        return fpclassify(x);
    }
}

template <typename Z, std::enable_if_t<std::is_integral_v<Z>, bool> = true>
inline constexpr int fpclassify(Z x)
{
    return boost::math::ccmath::fpclassify(static_cast<double>(x));
}

}

#endif // BOOST_MATH_CCMATH_FPCLASSIFY
