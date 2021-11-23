// Boost.Geometry
// Unit Test

// Copyright (c) 2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <limits>
#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/util/math.hpp>

namespace bgm = bg::math;

template <typename T>
void test_all()
{
    BOOST_CHECK(bgm::equals(0, 0));
    BOOST_CHECK(bgm::equals(1, 1));
    BOOST_CHECK(bgm::equals(123456, 123456));
    
    T eps = std::numeric_limits<T>::epsilon();
    if ( eps > 0 )
    {
        BOOST_CHECK(bgm::equals(0, 0+eps));
        BOOST_CHECK(bgm::equals(0+eps, 0));
        BOOST_CHECK(bgm::equals(1, 1+eps));
        BOOST_CHECK(bgm::equals(1+eps, 1));
        BOOST_CHECK(bgm::equals(12345+eps, 12345));
    }

    if (BOOST_GEOMETRY_CONDITION(std::numeric_limits<T>::has_infinity))
    {
        T inf = std::numeric_limits<T>::infinity();
        BOOST_CHECK(!bgm::equals(0, inf));
        BOOST_CHECK(!bgm::equals(0, -inf));
        BOOST_CHECK(!bgm::equals(1, inf));
        BOOST_CHECK(!bgm::equals(1, -inf));
        BOOST_CHECK(!bgm::equals(12345, inf));
        BOOST_CHECK(!bgm::equals(12345, -inf));
        BOOST_CHECK(!bgm::equals(inf, 0));
        BOOST_CHECK(!bgm::equals(-inf, 0));
        BOOST_CHECK(!bgm::equals(inf, 1));
        BOOST_CHECK(!bgm::equals(-inf, 1));
        BOOST_CHECK(!bgm::equals(inf, 12345));
        BOOST_CHECK(!bgm::equals(-inf, 12345));
        BOOST_CHECK(bgm::equals(inf, inf));
        BOOST_CHECK(bgm::equals(-inf, -inf));
        BOOST_CHECK(!bgm::equals(inf, -inf));
        BOOST_CHECK(!bgm::equals(-inf, inf));
    }

    if (BOOST_GEOMETRY_CONDITION(std::numeric_limits<T>::has_quiet_NaN))
    {
        T nan = std::numeric_limits<T>::quiet_NaN();
        BOOST_CHECK(!bgm::equals(0, nan));
        BOOST_CHECK(!bgm::equals(nan, 0));
        BOOST_CHECK(!bgm::equals(nan, nan));
        BOOST_CHECK(!bgm::equals(1, nan));
        BOOST_CHECK(!bgm::equals(nan, 1));
        BOOST_CHECK(!bgm::equals(12345, nan));
        BOOST_CHECK(!bgm::equals(nan, 12345));
    }
}

int test_main(int, char* [])
{
    test_all<int>();
    test_all<float>();
    test_all<double>();
    
    return 0;
}
