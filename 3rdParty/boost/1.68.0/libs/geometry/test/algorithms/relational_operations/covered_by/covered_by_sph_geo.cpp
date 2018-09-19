// Boost.Geometry

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_covered_by.hpp"


#include <boost/geometry/geometries/geometries.hpp>


template <typename P>
void test_point_box()
{
    typedef bg::model::box<P> box_t;

    test_geometry<P, box_t>("POINT(0 0)",    "BOX(0 0, 1 1)", true);
    test_geometry<P, box_t>("POINT(1 1)",    "BOX(0 0, 2 2)", true);

    test_geometry<P, box_t>("POINT(180 1)",  "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(-180 1)", "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(180 1)",  "BOX(170 0, 180 2)", true);
    test_geometry<P, box_t>("POINT(-180 1)", "BOX(170 0, 180 2)", true);
    test_geometry<P, box_t>("POINT(179 1)",  "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(-179 1)", "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(179 1)",  "BOX(170 0, 180 2)", true);
    test_geometry<P, box_t>("POINT(-179 1)", "BOX(170 0, 180 2)", false);
    test_geometry<P, box_t>("POINT(169 1)", "BOX(170 0, 180 2)", false);

    // https://svn.boost.org/trac/boost/ticket/12412
    test_geometry<P, box_t>("POINT(-0.127592 51.7)", "BOX(-2.08882 51.5034, -0.127592 51.9074)", true);
    // and related
    test_geometry<P, box_t>("POINT(-2.08882 51.7)", "BOX(-2.08882 51.5034, -0.127592 51.9074)", true);
    test_geometry<P, box_t>("POINT(0.127592 51.7)", "BOX(0.127592 51.5034, 2.08882 51.9074)", true);
    test_geometry<P, box_t>("POINT(2.08882 51.7)", "BOX(0.127592 51.5034, 2.08882 51.9074)", true);

    test_geometry<P, box_t>("POINT(179.08882 1)", "BOX(179.08882 0, 538.127592 2)", true);
    test_geometry<P, box_t>("POINT(178.127592 1)", "BOX(179.08882 0, 538.127592 2)", true);
    test_geometry<P, box_t>("POINT(179.08882 1)", "BOX(179.08882 0, 182.127592 2)", true);
    test_geometry<P, box_t>("POINT(-177.872408 1)", "BOX(179.08882 0, 182.127592 2)", true);
}

template <typename P>
void test_box_box()
{
    typedef bg::model::box<P> box_t;

    test_geometry<box_t, box_t>("BOX(0 0, 1 1)", "BOX(0 0, 1 1)", true);

    test_geometry<box_t, box_t>("BOX(-170 0,-160 1)", "BOX(-180 0, 180 1)", true);
    test_geometry<box_t, box_t>("BOX(-170 0,-160 1)", "BOX(170 0, 200 1)",  true);
    test_geometry<box_t, box_t>("BOX(-170 0,-150 1)", "BOX(170 0, 200 1)",  false);
    test_geometry<box_t, box_t>("BOX(0 0,1 1)",       "BOX(170 0, 370 1)",  true);
    test_geometry<box_t, box_t>("BOX(0 0,10 1)",      "BOX(170 0, 370 1)",  true);
    test_geometry<box_t, box_t>("BOX(-180 0,10 1)",   "BOX(170 0, 370 1)",  true);
    test_geometry<box_t, box_t>("BOX(-180 0,20 1)",   "BOX(170 0, 370 1)",  false);
    test_geometry<box_t, box_t>("BOX(10 0,20 1)",     "BOX(170 0, 370 1)",  false);
    test_geometry<box_t, box_t>("BOX(160 0,180 1)",   "BOX(170 0, 370 1)",  false);

    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(180 0, 190 1)",  true); // invalid?
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(180 0, 191 1)",  true); // invalid?
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(179 0, 190 1)",  true);
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(181 0, 190 1)",  false); // invalid?
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(180 0, 189 1)",  false); // invalid?

    // Related to https://svn.boost.org/trac/boost/ticket/12412
    test_geometry<box_t, box_t>("BOX(-1.346346 51.6, -0.127592 51.7)", "BOX(-2.08882 51.5034, -0.127592 51.9074)", true);
    test_geometry<box_t, box_t>("BOX(-2.08882 51.6, -1.346346 51.7)", "BOX(-2.08882 51.5034, -0.127592 51.9074)", true);
    test_geometry<box_t, box_t>("BOX(0.127592 51.6, 1.346346 51.7)", "BOX(0.127592 51.5034, 2.08882 51.9074)", true);
    test_geometry<box_t, box_t>("BOX(1.346346 51.6, 2.08882 51.7)", "BOX(0.127592 51.5034, 2.08882 51.9074)", true);

    test_geometry<box_t, box_t>("BOX(179.08882 1, 180.0 1)", "BOX(179.08882 0, 538.127592 2)", true);
    test_geometry<box_t, box_t>("BOX(177.0 1, 178.127592 1)", "BOX(179.08882 0, 538.127592 2)", true);
    test_geometry<box_t, box_t>("BOX(179.08882 1, 179.9 1)", "BOX(179.08882 0, 182.127592 2)", true);
    test_geometry<box_t, box_t>("BOX(-179.9 1, -177.872408 1)", "BOX(179.08882 0, 182.127592 2)", true);
}

template <typename P>
void test_point_polygon()
{
    typename boost::mpl::if_
        <
            boost::is_same<typename bg::cs_tag<P>::type, bg::geographic_tag>,
            bg::strategy::within::geographic_winding<P>,
            bg::strategy::within::spherical_winding<P>
        >::type s;

    typedef bg::model::polygon<P> poly;

    // MySQL report 08.2017
    test_geometry<P, poly>("POINT(-179 0)",
                           "POLYGON((0 0, 0 2, 2 0, 0 -2, 0 0))",
                           false);
    test_geometry<P, poly>("POINT(-179 0)",
                           "POLYGON((0 0, 0 2, 2 0, 0 -2, 0 0))",
                           false,
                           s);

    test_geometry<P, poly>("POINT(1 0)",
                           "POLYGON((0 0, 0 2, 2 0, 0 -2, 0 0))",
                           true);
    test_geometry<P, poly>("POINT(1 0)",
                           "POLYGON((0 0, 0 2, 2 0, 0 -2, 0 0))",
                           true,
                           s);
}

template <typename P>
void test_cs()
{
    test_point_box<P>();
    test_box_box<P>();
    test_point_polygon<P>();
}


int test_main( int , char* [] )
{
    test_cs<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_cs<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

#if defined(HAVE_TTMATH)
    test_cs<bg::model::point<ttmath_big, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_cs<bg::model::point<ttmath_big, 2, bg::cs::geographic<bg::degree> > >();;
#endif

    return 0;
}
