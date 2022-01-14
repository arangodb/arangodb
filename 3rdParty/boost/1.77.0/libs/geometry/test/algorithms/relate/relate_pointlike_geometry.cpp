// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2018 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2013, 2014, 2015, 2017.
// Modifications copyright (c) 2013-2017 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_relate.hpp"
#include "nan_cases.hpp"

//TEST
//#include <to_svg.hpp>

template <typename P>
void test_point_point()
{
    test_geometry<P, P>("POINT(0 0)", "POINT(0 0)", "0FFFFFFF2");
    test_geometry<P, P>("POINT(1 0)", "POINT(0 0)", "FF0FFF0F2");
}

template <typename P>
void test_point_multipoint()
{
    typedef bg::model::multi_point<P> mpt;

    test_geometry<P, mpt>("POINT(0 0)", "MULTIPOINT(0 0)", "0FFFFFFF2");
    test_geometry<P, mpt>("POINT(1 0)", "MULTIPOINT(0 0)", "FF0FFF0F2");
    test_geometry<P, mpt>("POINT(0 0)", "MULTIPOINT(0 0, 1 0)", "0FFFFF0F2");
}

template <typename P>
void test_multipoint_multipoint()
{
    typedef bg::model::multi_point<P> mpt;

    test_geometry<mpt, mpt>("MULTIPOINT(0 0)", "MULTIPOINT(0 0)", "0FFFFFFF2");
    test_geometry<mpt, mpt>("MULTIPOINT(1 0)", "MULTIPOINT(0 0)", "FF0FFF0F2");
    test_geometry<mpt, mpt>("MULTIPOINT(0 0)", "MULTIPOINT(0 0, 1 0)", "0FFFFF0F2");
    test_geometry<mpt, mpt>("MULTIPOINT(0 0, 1 0)", "MULTIPOINT(0 0)", "0F0FFFFF2");
    test_geometry<mpt, mpt>("MULTIPOINT(0 0, 1 1)", "MULTIPOINT(0 0, 1 0)", "0F0FFF0F2");

    //typedef bg::model::d2::point_xy<float> ptf;
    //typedef bg::model::multi_point<ptf> mptf;
    //test_geometry<mptf, mpt>("MULTIPOINT(0 0)", "MULTIPOINT(0 0)", "0FFFFFFF2");

    // assertion failure in relate->boundary_checker->std::equal_range with msvc
    if (BOOST_GEOMETRY_CONDITION(is_nan_case_supported<mpt>::value))
    {
        mpt g;
        std::string wkt;
        nan_case(g, wkt);

        check_geometry(g, g, wkt, wkt, "*********");
    }
}

template <typename P>
void test_point_linestring()
{
    typedef bg::model::linestring<P> ls;
    
    // degenerated
    //test_geometry<P, ls>("POINT(0 0)", "LINESTRING(0 0)", "0FFFFFFF2");

    test_geometry<P, ls>("POINT(0 0)", "LINESTRING(0 0, 2 2, 3 2)", "F0FFFF102");
    test_geometry<P, ls>("POINT(1 1)", "LINESTRING(0 0, 2 2, 3 2)", "0FFFFF102");
    test_geometry<P, ls>("POINT(3 2)", "LINESTRING(0 0, 2 2, 3 2)", "F0FFFF102");
    test_geometry<P, ls>("POINT(1 0)", "LINESTRING(0 0, 2 2, 3 2)", "FF0FFF102");

    test_geometry<P, ls>("POINT(0 0)", "LINESTRING(0 0, 2 2, 3 2, 0 0)", "0FFFFF1F2");
    test_geometry<P, ls>("POINT(1 1)", "LINESTRING(0 0, 2 2, 3 2, 0 0)", "0FFFFF1F2");
    test_geometry<P, ls>("POINT(3 2)", "LINESTRING(0 0, 2 2, 3 2, 0 0)", "0FFFFF1F2");
    test_geometry<P, ls>("POINT(1 0)", "LINESTRING(0 0, 2 2, 3 2, 0 0)", "FF0FFF1F2");
}

template <typename P>
void test_point_multilinestring()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<P, mls>("POINT(0 0)", "MULTILINESTRING((0 0, 2 0, 2 2),(0 0, 0 2))", "0FFFFF102");
    test_geometry<P, mls>("POINT(0 0)", "MULTILINESTRING((0 0, 2 0, 2 2),(0 0, 0 2, 2 2))", "0FFFFF1F2");
    test_geometry<P, mls>("POINT(0 0)", "MULTILINESTRING((0 0, 2 0, 2 2),(0 0, 0 2, 2 2),(0 0, 1 1))", "F0FFFF102");

    test_geometry<P, mls>("POINT(0 0)", "MULTILINESTRING((0 0,5 0),(0 0,0 5,5 0),(0 0,-5 0),(0 0,0 -5,-5 0))", "0FFFFF1F2");
    test_geometry<P, mls>("POINT(5 0)", "MULTILINESTRING((0 0,5 0),(0 0,0 5,5 0),(0 0,-5 0),(0 0,0 -5,-5 0))", "0FFFFF1F2");
    test_geometry<P, mls>("POINT(1 0)", "MULTILINESTRING((0 0,5 0),(0 0,0 5,5 0),(0 0,-5 0),(0 0,0 -5,-5 0))", "0FFFFF1F2");

    // assertion failure in relate->boundary_checker->std::equal_range with msvc
    if (BOOST_GEOMETRY_CONDITION(is_nan_case_supported<mls>::value))
    {
        // on the boundary
        std::string wkt0 = "POINT(3.1e+307 1)";
        P g0;
        bg::read_wkt(wkt0, g0);

        // disjoint
        std::string wkt1 = "POINT(1.1e+308 1.2e+308)";
        P g1;
        bg::read_wkt(wkt1, g1);

        mls g2;
        std::string wkt2;
        nan_case(g2, wkt2);

        check_geometry(g0, g2, wkt0, wkt2, "*********");
        check_geometry(g1, g2, wkt1, wkt2, "*********");
    }
}

template <typename P>
void test_multipoint_linestring()
{
    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::linestring<P> ls;
    
    // degenerated
    //test_geometry<mpt, ls>("MULTIPOINT(0 0)", "LINESTRING(0 0)", "0FFFFFFF2");

    test_geometry<mpt, ls>("MULTIPOINT(0 0)", "LINESTRING(0 0, 2 2, 3 2)", "F0FFFF102");
    test_geometry<mpt, ls>("MULTIPOINT(0 0, 1 1)", "LINESTRING(0 0, 2 2, 3 2)", "00FFFF102");
    test_geometry<mpt, ls>("MULTIPOINT(0 0, 1 1, 3 3)", "LINESTRING(0 0, 2 2, 3 2)", "000FFF102");
    test_geometry<mpt, ls>("MULTIPOINT(0 0, 3 3)", "LINESTRING(0 0, 2 2, 3 2)", "F00FFF102");
    test_geometry<mpt, ls>("MULTIPOINT(1 1, 3 3)", "LINESTRING(0 0, 2 2, 3 2)", "0F0FFF102");

    test_geometry<mpt, ls>("MULTIPOINT(0 0, 1 1)", "LINESTRING(0 0, 2 2, 3 2, 0 0)", "0FFFFF1F2");
    test_geometry<mpt, ls>("MULTIPOINT(0 0, 3 3)", "LINESTRING(0 0, 2 2, 3 2, 0 0)", "0F0FFF1F2");

    test_geometry<mpt, ls>("MULTIPOINT(0 0)", "LINESTRING(0 0, 2 2)", "F0FFFF102");
    test_geometry<mpt, ls>("MULTIPOINT(2 2)", "LINESTRING(0 0, 2 2)", "F0FFFF102");
    test_geometry<mpt, ls>("MULTIPOINT(0 0, 2 2)", "LINESTRING(0 0, 2 2)", "F0FFFF1F2");
    test_geometry<mpt, ls>("MULTIPOINT(0 0, 1 1, 0 1, 2 2)", "LINESTRING(0 0, 2 2)", "000FFF1F2");
}

template <typename P>
void test_multipoint_multilinestring()
{
    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;
    
    test_geometry<mpt, mls>("MULTIPOINT(0 0)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2))", "F0FFFF102");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 1 1)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2))", "00FFFF102");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 1 1, 3 3)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2))", "000FFF102");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 3 3)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2))", "F00FFF102");
    test_geometry<mpt, mls>("MULTIPOINT(1 1, 3 3)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2))", "0F0FFF102");

    test_geometry<mpt, mls>("MULTIPOINT(0 0, 1 1)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2, 0 0))", "0FFFFF1F2");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 3 3)", "MULTILINESTRING((0 0, 2 2),(2 2, 3 2, 0 0))", "0F0FFF1F2");

    test_geometry<mpt, mls>("MULTIPOINT(0 0,1 1)", "MULTILINESTRING((0 0,0 1,1 1),(1 1,1 0,0 0))", "0FFFFF1F2");

    test_geometry<mpt, mls>("MULTIPOINT(0 0)", "MULTILINESTRING((0 0,1 1),(1 1,2 2))", "F0FFFF102");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 1 1)", "MULTILINESTRING((0 0,1 1),(1 1,2 2))", "00FFFF102");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 1 1, 2 2)", "MULTILINESTRING((0 0,1 1),(1 1,2 2))", "00FFFF1F2");
    test_geometry<mpt, mls>("MULTIPOINT(0 0, 2 2)", "MULTILINESTRING((0 0,1 1),(1 1,2 2))", "F0FFFF1F2");
}

template <typename P>
void test_point_ring_polygon()
{
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> poly;
    
    test_geometry<P, ring>("POINT(0 0)", "POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))", "F0FFFF212");

    test_geometry<P, poly>("POINT(1 1)", "POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))", "0FFFFF212");
    test_geometry<P, poly>("POINT(1 3)", "POLYGON((0 0, 0 2, 2 2, 2 0, 0 0))", "FF0FFF212");
}

template <typename P>
void test_point_multipolygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<P, mpoly>("POINT(0 0)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "F0FFFF212");
    test_geometry<P, mpoly>("POINT(1 1)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "0FFFFF212");
    test_geometry<P, mpoly>("POINT(5 5)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "F0FFFF212");
    test_geometry<P, mpoly>("POINT(6 6)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "0FFFFF212");
    test_geometry<P, mpoly>("POINT(1 6)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "FF0FFF212");
}

template <typename P>
void test_multipoint_ring_polygon()
{
    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> poly;
    
    test_geometry<mpt, ring>("MULTIPOINT(0 0)", "POLYGON((0 0, 0 3, 3 3, 3 0, 0 0))", "F0FFFF212");
    test_geometry<mpt, ring>("MULTIPOINT(0 0, 1 1)", "POLYGON((0 0, 0 3, 3 3, 3 0, 0 0))", "00FFFF212");
    test_geometry<mpt, ring>("MULTIPOINT(0 0, 1 1, 4 4)", "POLYGON((0 0, 0 3, 3 3, 3 0, 0 0))", "000FFF212");
    test_geometry<mpt, ring>("MULTIPOINT(1 1, 4 4)", "POLYGON((0 0, 0 3, 3 3, 3 0, 0 0))", "0F0FFF212");

    test_geometry<mpt, poly>("MULTIPOINT(2 2)", "POLYGON((0 0, 0 4, 4 4, 4 0, 0 0),(1 1, 3 1, 3 3, 1 3, 1 1))", "FF0FFF212");
    test_geometry<mpt, poly>("MULTIPOINT(0 0, 1 1)", "POLYGON((0 0, 0 4, 4 4, 4 0, 0 0),(1 1, 3 1, 3 3, 1 3, 1 1))", "F0FFFF212");
    test_geometry<mpt, poly>("MULTIPOINT(0 0, 1 1, 2 2)", "POLYGON((0 0, 0 4, 4 4, 4 0, 0 0),(1 1, 3 1, 3 3, 1 3, 1 1))", "F00FFF212");
    test_geometry<mpt, poly>("MULTIPOINT(2 2, 4 4)", "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(1 1, 3 1, 3 3, 1 3, 1 1))", "0F0FFF212");
}

template <typename P>
void test_multipoint_multipolygon()
{
    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;
    
    test_geometry<mpt, mpoly>("MULTIPOINT(0 0)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "F0FFFF212");
    test_geometry<mpt, mpoly>("MULTIPOINT(0 0, 1 1)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "00FFFF212");
    test_geometry<mpt, mpoly>("MULTIPOINT(1 1, 5 5)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "00FFFF212");
    test_geometry<mpt, mpoly>("MULTIPOINT(1 6, 6 6)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "0F0FFF212");
    test_geometry<mpt, mpoly>("MULTIPOINT(0 0, 1 6)", "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0)),((5 5, 5 9, 9 9, 9 5, 5 5)))", "F00FFF212");
}

template <typename P>
void test_all()
{
    test_point_point<P>();
    test_point_multipoint<P>();
    test_multipoint_multipoint<P>();

    test_point_linestring<P>();
    test_point_multilinestring<P>();
    test_multipoint_linestring<P>();
    test_multipoint_multilinestring<P>();

    test_point_ring_polygon<P>();
    test_point_multipolygon<P>();
    test_multipoint_ring_polygon<P>();
    test_multipoint_multipolygon<P>();
}

int test_main( int , char* [] )
{
    check_mask();

    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
