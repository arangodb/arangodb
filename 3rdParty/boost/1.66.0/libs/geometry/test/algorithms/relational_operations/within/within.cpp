// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014, 2015, 2016.
// Modifications copyright (c) 2014-2016 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_within.hpp"


#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

template <typename P1, typename P2>
void test_point_box()
{
    typedef bg::model::box<P1> box_type1;
    typedef bg::model::box<P2> box_type2;

    test_geometry<P1, box_type2>("POINT(1 1)", "BOX(0 0,2 2)", true);
    test_geometry<P1, box_type2>("POINT(0 0)", "BOX(0 0,2 2)", false);
    test_geometry<P1, box_type2>("POINT(2 2)", "BOX(0 0,2 2)", false);
    test_geometry<P1, box_type2>("POINT(0 1)", "BOX(0 0,2 2)", false);
    test_geometry<P1, box_type2>("POINT(1 0)", "BOX(0 0,2 2)", false);

    test_geometry<P1, box_type2>("POINT(3 3)", "BOX(1 1,4 4)", true);
    test_geometry<P2, box_type1>("POINT(3 3)", "BOX(0 0,5 5)", true);

    test_geometry<box_type1, box_type2>("BOX(1 1,2 2)", "BOX(0 0,3 3)", true);
    test_geometry<box_type1, box_type2>("BOX(0 0,3 3)", "BOX(1 1,2 2)", false);

    test_geometry<box_type1, box_type2>("BOX(1 1,3 3)", "BOX(0 0,3 3)", true);
    test_geometry<box_type1, box_type2>("BOX(3 1,3 3)", "BOX(0 0,3 3)", false);

    test_geometry<box_type1, box_type2>("BOX(1 1,4 4)", "BOX(0 0,5 5)", true);
    test_geometry<box_type2, box_type1>("BOX(0 0,5 5)", "BOX(1 1,4 4)", false);

    /*
    test_within_code<P, box_type>("POINT(1 1)", "BOX(0 0,2 2)", 1);
    test_within_code<P, box_type>("POINT(1 0)", "BOX(0 0,2 2)", 0);
    test_within_code<P, box_type>("POINT(0 1)", "BOX(0 0,2 2)", 0);
    test_within_code<P, box_type>("POINT(0 3)", "BOX(0 0,2 2)", -1);
    test_within_code<P, box_type>("POINT(3 3)", "BOX(0 0,2 2)", -1);

    test_within_code<box_type, box_type>("BOX(1 1,2 2)", "BOX(0 0,3 3)", 1);
    test_within_code<box_type, box_type>("BOX(0 1,2 2)", "BOX(0 0,3 3)", 0);
    test_within_code<box_type, box_type>("BOX(1 0,2 2)", "BOX(0 0,3 3)", 0);
    test_within_code<box_type, box_type>("BOX(1 1,2 3)", "BOX(0 0,3 3)", 0);
    test_within_code<box_type, box_type>("BOX(1 1,3 2)", "BOX(0 0,3 3)", 0);
    test_within_code<box_type, box_type>("BOX(1 1,3 4)", "BOX(0 0,3 3)", -1);
    */
}

void test_point_box_3d()
{
    typedef boost::geometry::model::point<double, 3, boost::geometry::cs::cartesian> point_type;
    typedef boost::geometry::model::box<point_type> box_type;
    box_type box(point_type(0, 0, 0), point_type(4, 4, 4));
    BOOST_CHECK_EQUAL(bg::within(point_type(2, 2, 2), box), true);
    BOOST_CHECK_EQUAL(bg::within(point_type(2, 4, 2), box), false);
    BOOST_CHECK_EQUAL(bg::within(point_type(2, 2, 4), box), false);
    BOOST_CHECK_EQUAL(bg::within(point_type(2, 2, 5), box), false);

    box_type box2(point_type(2, 2, 2), point_type(3, 3, 3));
    BOOST_CHECK_EQUAL(bg::within(box2, box), true);

}

template <typename P1, typename P2>
void test_point_poly()
{
    typedef boost::geometry::model::polygon<P1> poly1;
    typedef boost::geometry::model::polygon<P2> poly2;

    test_geometry<P1, poly2>("POINT(3 3)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<P2, poly1>("POINT(3 3)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
}

template <typename P1, typename P2>
void test_all()
{
    test_point_box<P1, P2>();
    test_point_poly<P1, P2>();
}

template <typename P>
void test_all()
{
    test_all<P, P>();
}

void test_strategy()
{
    // Test by explicitly specifying a strategy
    typedef bg::model::d2::point_xy<double> point_type;
    typedef bg::model::box<point_type> box_type;
    point_type p(3, 3);
    box_type b(point_type(0, 0), point_type(5, 5));
    box_type b0(point_type(0, 0), point_type(5, 0));

    bool r = bg::within(p, b,
        bg::strategy::within::point_in_box<point_type, box_type>());
    BOOST_CHECK_EQUAL(r, true);

    r = bg::within(b, b,
        bg::strategy::within::box_in_box<box_type, box_type>());
    BOOST_CHECK_EQUAL(r, true);

    r = bg::within(b0, b0,
        bg::strategy::within::box_in_box<box_type, box_type>());
    BOOST_CHECK_EQUAL(r, false);

    r = bg::within(p, b,
        bg::strategy::within::point_in_box_by_side<point_type, box_type>());
    BOOST_CHECK_EQUAL(r, true);
}


int test_main( int , char* [] )
{
    typedef boost::geometry::model::d2::point_xy<double> xyd;
    typedef boost::geometry::model::d2::point_xy<float> xyf;
    typedef boost::geometry::model::d2::point_xy<int> xyi;
    typedef boost::geometry::model::point<double, 2, boost::geometry::cs::cartesian> p2d;
    
    test_all<xyd, p2d>();
    test_all<xyf, p2d>();
    test_all<xyi, xyd>();

    test_all<xyi>();
    test_all<xyd>();

    test_point_box_3d();
    test_strategy();


#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
