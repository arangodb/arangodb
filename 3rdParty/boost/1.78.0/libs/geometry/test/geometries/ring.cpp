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
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/concepts/ring_concept.hpp>
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
bg::model::ring<P> create_ring()
{   
    bg::model::ring<P> r1;
    P p1;
    P p2;
    P p3;
    P p4;
    bg::assign_values(p1, 2, 2);
    bg::assign_values(p2, 2, 0);
    bg::assign_values(p3, 0, 0);
    bg::assign_values(p4, 0, 2);
    
    bg::append(r1, p1);
    bg::append(r1, p2);
    bg::append(r1, p3);
    bg::append(r1, p4);
    bg::append(r1, p1);
    return r1;
}

template <typename P, typename T>
void check_point(P& to_check, T x, T y)
{
    BOOST_CHECK_EQUAL(bg::get<0>(to_check), x);
    BOOST_CHECK_EQUAL(bg::get<1>(to_check), y);
}

template <typename R, typename P>
void check_ring(R& to_check, P p1, P p2, P p3, P p4)
{   
    check_point(to_check[0], bg::get<0>(p1), bg::get<1>(p1));
    check_point(to_check[1], bg::get<0>(p2), bg::get<1>(p2));
    check_point(to_check[2], bg::get<0>(p3), bg::get<1>(p3));
    check_point(to_check[3], bg::get<0>(p4), bg::get<1>(p4));
    check_point(to_check[4], bg::get<0>(p1), bg::get<1>(p1));
}

template <typename P>
void test_default_constructor()
{
    bg::model::ring<P> r1(create_ring<P>());
    check_ring(r1, P(2, 2), P(2, 0), P(0, 0), P(0, 2));
}

template <typename P>
void test_copy_constructor()
{
    bg::model::ring<P> r1 = create_ring<P>();
    check_ring(r1, P(2, 2), P(2, 0), P(0, 0), P(0, 2));
}

template <typename P>
void test_copy_assignment()
{
    bg::model::ring<P> r1(create_ring<P>()), r2;
    r2 = r1;
    check_ring(r2, P(2, 2), P(2, 0), P(0, 0), P(0, 2));
}

template <typename P>
void test_concept()
{   
    typedef bg::model::ring<P> R;

    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstRing<R>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::Ring<R>) );

    typedef typename bg::coordinate_type<R>::type T;
    typedef typename bg::point_type<R>::type PR;
    boost::ignore_unused<T, PR>();
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
void test_custom_ring(bg::model::ring<P> IL)
{   
    bg::model::ring<P> r1(IL);
    std::ostringstream out;
    out << bg::dsv(r1);
    BOOST_CHECK_EQUAL(out.str(), "((3, 3), (3, 0), (0, 0), (0, 3), (3, 3))");
}

template <typename P>
void test_custom()
{   
#ifdef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
    std::initializer_list<P> IL = {P(3, 3), P(3, 0), P(0, 0), P(0, 3), P(3, 3)};
    test_custom_ring<P>(IL);
#endif//BOOST_NO_CXX11_HDR_INITIALIZER_LIST
}

template <typename CS>
void test_cs()
{
    test_all<bg::model::point<int, 2, CS> >();
    test_all<bg::model::point<float, 2, CS> >();
    test_all<bg::model::point<double, 2, CS> >();

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
