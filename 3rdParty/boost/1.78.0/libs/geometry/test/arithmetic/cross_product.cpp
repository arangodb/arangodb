// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>

#include <boost/geometry/arithmetic/cross_product.hpp>

#include <boost/geometry/algorithms/assign.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <test_common/test_point.hpp>


template <typename P>
void test_2d()
{
    P p1;
    bg::assign_values(p1, 20, 30);
    P p2;
    bg::assign_values(p2, 45, 70);
    P c = bg::cross_product(p1, p2);

    typedef typename bg::coordinate_type<P>::type scalar_type;
    BOOST_CHECK_EQUAL(bg::get<0>(c), scalar_type(50));
}

template <typename P>
void test_3d()
{
    P p1;
    bg::assign_values(p1, 20, 30, 10);
    P p2;
    bg::assign_values(p2, 45, 70, 20);
    P c = bg::cross_product(p1, p2);

    typedef typename bg::coordinate_type<P>::type scalar_type;
    BOOST_CHECK_EQUAL(bg::get<0>(c), scalar_type(-100));
    BOOST_CHECK_EQUAL(bg::get<1>(c), scalar_type(50));
    BOOST_CHECK_EQUAL(bg::get<2>(c), scalar_type(50));
}

template <typename P>
void test_constexpr()
{
    constexpr P p1 = P(20, 30, 10);
    constexpr P p2 = P(45, 70, 20);
    constexpr P c = bg::cross_product(p1, p2);
    constexpr auto c0 = bg::get<0>(c);
    constexpr auto c1 = bg::get<1>(c);
    constexpr auto c2 = bg::get<2>(c);
    BOOST_CHECK_EQUAL(c0, -100);
    BOOST_CHECK_EQUAL(c1, 50);
    BOOST_CHECK_EQUAL(c2, 50);
}

#ifdef TEST_FAIL_CROSS_PRODUCT
template <typename P>
void test_4d()
{
    P p1;
    bg::assign_values(p1, 20, 30, 10);
    bg::set<3>(p1, 15);
    P p2;
    bg::assign_values(p2, 45, 70, 20);
    bg::set<3>(p2, 35);
    P c = bg::cross_product(p1, p2);
    boost::ignore_unused(c);
}
#endif

int test_main(int, char* [])
{
    test_2d<bg::model::point<int, 2, bg::cs::cartesian> >();
    test_2d<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_2d<bg::model::point<double, 2, bg::cs::cartesian> >();

    test_3d<bg::model::point<int, 3, bg::cs::cartesian> >();
    test_3d<bg::model::point<float, 3, bg::cs::cartesian> >();
    test_3d<bg::model::point<double, 3, bg::cs::cartesian> >();

#ifdef TEST_FAIL_CROSS_PRODUCT
    test_4d<bg::model::point<int, 4, bg::cs::cartesian> >();
    test_4d<bg::model::point<float, 4, bg::cs::cartesian> >();
    test_4d<bg::model::point<double, 4, bg::cs::cartesian> >();
#endif

    test_constexpr<bg::model::point<double, 3, bg::cs::cartesian> >();

    return 0;
}

