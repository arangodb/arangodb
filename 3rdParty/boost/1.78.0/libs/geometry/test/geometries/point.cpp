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

#include <boost/core/ignore_unused.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/geometries/concepts/point_concept.hpp>
#include <boost/geometry/geometries/point.hpp>

#include <test_common/test_point.hpp>


template <typename T, typename CS>
bg::model::point<T, 3, CS> create_point()
{
    T t1 = 1;
    T t2 = 2;
    T t3 = 3;
    return bg::model::point<T, 3, CS>(t1, t2, t3);
}

template <typename P, typename T>
void check_point(P& to_check, T x, T y, T z)
{
    BOOST_CHECK_EQUAL(bg::get<0>(to_check), x);
    BOOST_CHECK_EQUAL(bg::get<1>(to_check), y);
    BOOST_CHECK_EQUAL(bg::get<2>(to_check), z);
}

template <typename T, typename CS>
void test_default_constructor()
{
    bg::model::point<T, 3, CS> p(create_point<T, CS>());
    check_point(p, 1, 2, 3);
}

template <typename T, typename CS>
void test_copy_constructor()
{
    bg::model::point<T, 3, CS> p = create_point<T, CS>();
    check_point(p, 1, 2, 3);
}

template <typename T, typename CS>
void test_copy_assignment()
{
    bg::model::point<T, 3, CS> p(create_point<T, CS>());
    bg::set<0>(p, 4);
    bg::set<1>(p, 5);
    bg::set<2>(p, 6);
    check_point(p, 4, 5, 6);
}

template <typename T, typename CS>
void test_concept()
{
    typedef bg::model::point<T, 3, CS> P;

    // Compilation tests, all things should compile.
    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstPoint<P>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::Point<P>) );

    typedef typename bg::coordinate_type<P>::type T1;
    boost::ignore_unused<T1>();
}

template <typename T, typename CS>
void test_constexpr()
{
    typedef bg::model::point<T, 3, CS> P;
    constexpr P p1 = P(1, 2, 3);
    constexpr P p2 = P(4, 5);
    constexpr T c1 = bg::get<0>(p1);
    constexpr T c2 = bg::get<2>(p2);
    BOOST_CHECK_EQUAL(c1, 1);
    BOOST_CHECK_EQUAL(c2, 0);
    check_point(p1, 1, 2, 3);
    check_point(p2, 4, 5, 0);
}

template <typename T, typename CS>
void test_all()
{
    test_default_constructor<T, CS>();
    test_copy_constructor<T, CS>();
    test_copy_assignment<T, CS>();
    test_concept<T, CS>();
    test_constexpr<T, CS>();
}

template <typename CS>
void test_cs()
{
    test_all<int, CS>();
    test_all<float, CS>();
    test_all<double, CS>();
}

int test_main(int, char* [])
{   
    test_cs<bg::cs::cartesian>();
    test_cs<bg::cs::spherical<bg::degree> >();
    test_cs<bg::cs::spherical_equatorial<bg::degree> >();
    test_cs<bg::cs::geographic<bg::degree> >();

    return 0;
}
