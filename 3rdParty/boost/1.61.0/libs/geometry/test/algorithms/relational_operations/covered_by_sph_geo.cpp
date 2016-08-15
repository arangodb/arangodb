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
}


template <typename P>
void test_cs()
{
    test_point_box<P>();
    test_box_box<P>();
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
