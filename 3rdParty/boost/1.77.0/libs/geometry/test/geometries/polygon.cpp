// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Digvijay Janartha, Hamirpur, India.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

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
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/concepts/polygon_concept.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <test_common/test_point.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)

template <typename P>
bg::model::polygon<P> create_polygon()
{   
    bg::model::polygon<P> pl1;
    P p1;
    P p2;
    P p3;
    bg::assign_values(p1, 1, 2);
    bg::assign_values(p2, 2, 0);
    bg::assign_values(p3, 0, 0);
    
    bg::append(pl1, p1);
    bg::append(pl1, p2);
    bg::append(pl1, p3);
    bg::append(pl1, p1);
    return pl1;
}

template <typename PL, typename P>
void check_polygon(PL& to_check, P p1, P p2, P p3)
{   
    PL cur;
    bg::append(cur, p1);
    bg::append(cur, p2);
    bg::append(cur, p3);
    bg::append(cur, p1);

    std::ostringstream out1, out2;
    out1 << bg::dsv(to_check);
    out2 << bg::dsv(cur);
    BOOST_CHECK_EQUAL(out1.str(), out2.str());
}

template <typename P>
void test_default_constructor()
{
    bg::model::polygon<P> pl1(create_polygon<P>());
    check_polygon(pl1, P(1, 2), P(2, 0), P(0, 0));
}

template <typename P>
void test_copy_constructor()
{
    bg::model::polygon<P> pl1 = create_polygon<P>();
    check_polygon(pl1, P(1, 2), P(2, 0), P(0, 0));
}

template <typename P>
void test_copy_assignment()
{
    bg::model::polygon<P> pl1(create_polygon<P>()), pl2;
    pl2 = pl1;
    check_polygon(pl2, P(1, 2), P(2, 0), P(0, 0));
}

template <typename P>
void test_concept()
{   
    typedef bg::model::polygon<P> PL;

    BOOST_CONCEPT_ASSERT( (bg::concepts::ConstPolygon<PL>) );
    BOOST_CONCEPT_ASSERT( (bg::concepts::Polygon<PL>) );

    typedef typename bg::coordinate_type<PL>::type T;
    typedef typename bg::point_type<PL>::type PPL;
    boost::ignore_unused<T, PPL>();
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
void test_custom_polygon(bg::model::ring<P> IL)
{   
    std::initializer_list<bg::model::ring<P> > RIL = {IL};
    bg::model::polygon<P> pl1(RIL);
    std::ostringstream out;
    out << bg::dsv(pl1);
    BOOST_CHECK_EQUAL(out.str(), "(((3, 3), (3, 0), (0, 0), (0, 3), (3, 3)))");
}

template <typename P>
void test_custom()
{   
    std::initializer_list<P> IL = {P(3, 3), P(3, 0), P(0, 0), P(0, 3), P(3, 3)};
    bg::model::ring<P> r1(IL);
    test_custom_polygon<P>(r1);
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
