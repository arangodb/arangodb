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
#include <boost/geometry/geometries/point_xyz.hpp>

#include <test_common/test_point.hpp>


template <typename T>
bg::model::d3::point_xyz<T> create_point_xyz()
{
    T t1 = 1;
    T t2 = 2;
    T t3 = 3;
    return bg::model::d3::point_xyz<T>(t1, t2, t3);
}

template <typename P, typename T>
void check_point_xyz(P& to_check, T x, T y, T z)
{
    BOOST_CHECK_EQUAL(bg::get<0>(to_check), x);
    BOOST_CHECK_EQUAL(bg::get<1>(to_check), y);
    BOOST_CHECK_EQUAL(bg::get<2>(to_check), z);
    BOOST_CHECK_EQUAL(to_check.x(), x);
    BOOST_CHECK_EQUAL(to_check.y(), y);
    BOOST_CHECK_EQUAL(to_check.z(), z);
}

template <typename T>
void test_default_constructor()
{
    bg::model::d3::point_xyz<T> p(create_point_xyz<T>());
    check_point_xyz(p, 1, 2, 3);
}

template <typename T>
void test_copy_constructor()
{
    bg::model::d3::point_xyz<T> p = create_point_xyz<T>();
    check_point_xyz(p, 1, 2, 3);
}

template <typename T>
void test_copy_assignment()
{
    bg::model::d3::point_xyz<T> p(create_point_xyz<T>());
    bg::set<0>(p, 4);
    bg::set<1>(p, 5);
    bg::set<2>(p, 6);
    check_point_xyz(p, 4, 5, 6);
}

template <typename T>
void test_constexpr()
{
    typedef bg::model::d3::point_xyz<T> P;
    constexpr P p = P(1, 2, 3);
    constexpr T c = bg::get<0>(p);
    BOOST_CHECK_EQUAL(c, 1);
    check_point_xyz(p, 1, 2, 3);
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
