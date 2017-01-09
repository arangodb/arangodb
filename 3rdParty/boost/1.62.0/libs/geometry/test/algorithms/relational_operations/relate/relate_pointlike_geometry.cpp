// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013, 2014, 2015.
// Modifications copyright (c) 2013-2015 Oracle and/or its affiliates.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

#include "test_relate.hpp"
#include "../nan_cases.hpp"

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
void test_all()
{
    test_point_point<P>();
    test_point_multipoint<P>();
    test_multipoint_multipoint<P>();
    test_point_linestring<P>();
    test_point_multilinestring<P>();
}

int test_main( int , char* [] )
{
    check_mask();

    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
