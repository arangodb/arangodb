// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_math_sqrt
#endif

#include <cmath>
#include <iostream>
#include <type_traits>

#include <boost/test/included/unit_test.hpp>

#include <boost/config.hpp>

#include "number_types.hpp"

// important: the include above must precede the include below,
// otherwise the test will fail for the custom number type:
// custom_with_global_sqrt

#include <boost/geometry/algorithms/not_implemented.hpp>
#include <boost/geometry/util/math.hpp>

namespace bg = boost::geometry;




// call BOOST_CHECK
template
<
    typename Argument, typename Result,
    std::enable_if_t<std::is_fundamental<Argument>::value, int> = 0
>
inline void check(Argument const& arg, Result const& result)
{
    BOOST_CHECK_CLOSE(static_cast<double>(bg::math::sqrt(arg)),
                      static_cast<double>(result),
                      0.00001);        
}

template
<
    typename Argument, typename Result,
    std::enable_if_t<! std::is_fundamental<Argument>::value, int> = 0
>
inline void check(Argument const& arg, Result const& result)
{
    Result const tol(0.00001);
    BOOST_CHECK( bg::math::abs(bg::math::sqrt(arg) - result) < tol );
}


// test sqrt return type and value
template <typename Argument, typename Result>
inline void check_sqrt(Argument const& arg, Result const& result)
{
    using return_type = typename bg::math::detail::square_root<Argument>::return_type;
    BOOST_GEOMETRY_STATIC_ASSERT((std::is_same<return_type, Result>::value),
                                 "Wrong return type",
                                 return_type, Result);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "testing: " << typeid(Result).name()
                << " sqrt(" << typeid(Argument).name()
                << ")" << std::endl;
#endif
    check<Argument, Result>(arg, result);
}


// test cases
BOOST_AUTO_TEST_CASE( test_math_sqrt_fundamental )
{
    static const double sqrt2 = std::sqrt(2.0);
    static const long double sqrt2L = std::sqrt(2.0L);
    static const float sqrt2F = std::sqrt(2.0F);

    check_sqrt<float, float>(2.0F, sqrt2F);
    check_sqrt<double, double>(2.0, sqrt2);
    check_sqrt<long double, long double>(2.0L, sqrt2L);

    check_sqrt<char, double>(2, sqrt2);
    check_sqrt<signed char, double>(2, sqrt2);
    check_sqrt<short, double>(2, sqrt2);
    check_sqrt<int, double>(2, sqrt2);
    check_sqrt<long, double>(2L, sqrt2);

    check_sqrt<long long, double>(2LL, sqrt2);
}


BOOST_AUTO_TEST_CASE( test_math_sqrt_custom )
{
    typedef number_types::custom<double> custom1;
    typedef custom_global<double> custom2;
    typedef number_types::custom_with_global_sqrt<double> custom3;

    static const double sqrt2 = std::sqrt(2.0);

    check_sqrt<custom1, custom1>(custom1(2.0), custom1(sqrt2));
    check_sqrt<custom2, custom2>(custom2(2.0), custom2(sqrt2));
    check_sqrt<custom3, custom3>(custom3(2.0), custom3(sqrt2));
}
