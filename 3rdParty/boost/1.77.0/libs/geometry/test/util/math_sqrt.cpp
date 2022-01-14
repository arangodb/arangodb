// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_math_sqrt
#endif

#include <cmath>
#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include <boost/config.hpp>
#include <boost/type_traits/is_fundamental.hpp>

#include "number_types.hpp"

// important: the include above must precede the include below,
// otherwise the test will fail for the custom number type:
// custom_with_global_sqrt

#include <boost/geometry/util/math.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>

namespace bg = boost::geometry;




// call BOOST_CHECK
template <typename Argument, bool IsFundamental /* true */>
struct check
{
    template <typename Result>
    static inline void apply(Argument const& arg, Result const& result)
    {
        BOOST_CHECK_CLOSE(static_cast<double>(bg::math::sqrt(arg)),
                          static_cast<double>(result),
                          0.00001);        
    }
};


template <typename Argument>
struct check<Argument, false>
{
    template <typename Result>
    static inline void apply(Argument const& arg, Result const& result)
    {
        Result const tol(0.00001);
        BOOST_CHECK( bg::math::abs(bg::math::sqrt(arg) - result) < tol );
    }
};






// test sqrt return type and value
template
<
    typename Argument,
    typename ExpectedResult,
    typename Result = typename bg::math::detail::square_root
        <
            Argument
        >::return_type,
    bool IsFundamental = boost::is_fundamental<Argument>::value
>
struct check_sqrt
    : bg::not_implemented<Argument, Result, ExpectedResult>
{};


template <typename Argument, typename Result, bool IsFundamental>
struct check_sqrt<Argument, Result, Result, IsFundamental>
{
    static inline void apply(Argument const& arg, Result const& result)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "testing: " << typeid(Result).name()
                  << " sqrt(" << typeid(Argument).name()
                  << ")" << std::endl;
#endif
        check<Argument, IsFundamental>::apply(arg, result);
    }
};






// test cases
BOOST_AUTO_TEST_CASE( test_math_sqrt_fundamental )
{
    static const double sqrt2 = std::sqrt(2.0);
    static const long double sqrt2L = std::sqrt(2.0L);
    static const float sqrt2F = std::sqrt(2.0F);

    check_sqrt<float, float>::apply(2.0F, sqrt2F);
    check_sqrt<double, double>::apply(2.0, sqrt2);
    check_sqrt<long double, long double>::apply(2.0L, sqrt2L);

    check_sqrt<char, double>::apply(2, sqrt2);
    check_sqrt<signed char, double>::apply(2, sqrt2);
    check_sqrt<short, double>::apply(2, sqrt2);
    check_sqrt<int, double>::apply(2, sqrt2);
    check_sqrt<long, double>::apply(2L, sqrt2);
#if !defined(BOOST_NO_LONG_LONG)
    check_sqrt<long long, double>::apply(2LL, sqrt2);
#endif
#ifdef BOOST_HAS_LONG_LONG
    check_sqrt
        <
            boost::long_long_type, double
        >::apply(boost::long_long_type(2), sqrt2);
#endif
}


BOOST_AUTO_TEST_CASE( test_math_sqrt_custom )
{
    typedef number_types::custom<double> custom1;
    typedef custom_global<double> custom2;
    typedef number_types::custom_with_global_sqrt<double> custom3;

    static const double sqrt2 = std::sqrt(2.0);

    check_sqrt<custom1, custom1>::apply(custom1(2.0), custom1(sqrt2));
    check_sqrt<custom2, custom2>::apply(custom2(2.0), custom2(sqrt2));
    check_sqrt<custom3, custom3>::apply(custom3(2.0), custom3(sqrt2));
}
