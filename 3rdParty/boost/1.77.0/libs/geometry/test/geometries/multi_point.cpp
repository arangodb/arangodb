// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/append.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/concepts/multi_point_concept.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <test_common/test_point.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)

#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
#include <initializer_list>
#endif//BOOST_NO_CXX11_HDR_INITIALIZER_LIST

template <typename P>
bg::model::multi_point<P> create_multi_point()
{   
    bg::model::multi_point<P> mp1;
    P p1;
    bg::assign_values(p1, 1, 2, 3);
    bg::append(mp1, p1);
    return mp1;
}

template <typename L, typename T>
void check_multi_point(L& to_check, T x, T y, T z)
{
    BOOST_CHECK_EQUAL(bg::get<0>(to_check[0]), x);
    BOOST_CHECK_EQUAL(bg::get<1>(to_check[0]), y);
    BOOST_CHECK_EQUAL(bg::get<2>(to_check[0]), z);
}

template <typename P>
void test_default_constructor()
{
    bg::model::multi_point<P> mp1(create_multi_point<P>());
    check_multi_point(mp1, 1, 2, 3);
}

template <typename P>
void test_copy_constructor()
{
    bg::model::multi_point<P> mp1 = create_multi_point<P>();
    check_multi_point(mp1, 1, 2, 3);
}

template <typename P>
void test_copy_assignment()
{
    bg::model::multi_point<P> mp1(create_multi_point<P>()), mp2;
    mp2 = mp1;
    check_multi_point(mp2, 1, 2, 3);
}

template <typename P>
void test_concept()
{   
    typedef bg::model::multi_point<P> MP;

    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstMultiPoint<MP>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::MultiPoint<MP>) );

    typedef typename bg::coordinate_type<MP>::type T;
    typedef typename bg::point_type<MP>::type MPP;
    boost::ignore_unused<T, MPP>();
}

template <typename P>
void test_all()
{   
    test_default_constructor<P>();
    test_copy_constructor<P>();
    test_copy_assignment<P>();
    test_concept<P>();
}

template <typename P>
void test_custom_multi_point(std::initializer_list<P> IL)
{
    bg::model::multi_point<P> mp1(IL);
    std::ostringstream out;
    out << bg::dsv(mp1);
    BOOST_CHECK_EQUAL(out.str(), "((0, 0), (1, 2), (2, 0))");
}

template <typename P>
void test_custom()
{   
#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
    std::initializer_list<P> IL = {P(0, 0), P(1, 2), P(2, 0)};
    test_custom_multi_point<P>(IL);
#endif//BOOST_NO_CXX11_HDR_INITIALIZER_LIST
}

template <typename CS>
void test_cs()
{
    test_all<bg::model::point<int, 3, CS> >();
    test_all<bg::model::point<float, 3, CS> >();
    test_all<bg::model::point<double, 3, CS> >();

    test_custom<bg::model::point<double, 2, CS> >();
}


int test_main(int, char* [])
{   
    test_cs<bg::cs::cartesian>();
    test_cs<bg::cs::spherical<bg::degree> >();
    test_cs<bg::cs::spherical_equatorial<bg::degree> >();
    test_cs<bg::cs::geographic<bg::degree> >();

    test_custom<bg::model::d2::point_xy<double> >();

    return 0;
}
