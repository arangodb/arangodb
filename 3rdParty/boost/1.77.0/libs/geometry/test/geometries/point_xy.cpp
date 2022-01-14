// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <test_common/test_point.hpp>


template <typename T>
bg::model::d2::point_xy<T> create_point_xy()
{
    T t1 = 1;
    T t2 = 2;
    return bg::model::d2::point_xy<T>(t1, t2);
}

template <typename P, typename T>
void check_point_xy(P& to_check, T x, T y)
{
    BOOST_CHECK_EQUAL(bg::get<0>(to_check), x);
    BOOST_CHECK_EQUAL(bg::get<1>(to_check), y);
    BOOST_CHECK_EQUAL(to_check.x(), x);
    BOOST_CHECK_EQUAL(to_check.y(), y);
}

template <typename T>
void test_default_constructor()
{
    bg::model::d2::point_xy<T> p(create_point_xy<T>());
    check_point_xy(p, 1, 2);
}

template <typename T>
void test_copy_constructor()
{
    bg::model::d2::point_xy<T> p(create_point_xy<T>());
    check_point_xy(p, 1, 2);
}

template <typename T>
void test_copy_assignment()
{
    bg::model::d2::point_xy<T> p(create_point_xy<T>());
    bg::set<0>(p, 4);
    bg::set<1>(p, 5);
    check_point_xy(p, 4, 5);
}

template <typename T>
void test_constexpr()
{
    typedef bg::model::d2::point_xy<T> P;
    constexpr P p = P(1, 2);
    constexpr T c = bg::get<0>(p);
    BOOST_CHECK_EQUAL(c, 1);
    check_point_xy(p, 1, 2);
}

template <typename T>
void test_all()
{
    test_default_constructor<T>();
    test_copy_constructor<T>();
    test_copy_assignment<T>();
    test_constexpr<T>();
}

int test_main(int, char* [])
{
    test_all<int>();
    test_all<float>();
    test_all<double>();

    return 0;
}
