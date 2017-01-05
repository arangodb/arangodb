// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_union_pointlike_pointlike
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define BOOST_GEOMETRY_DEBUG_TURNS
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#endif

#include <boost/test/included/unit_test.hpp>

#include "../test_set_ops_pointlike.hpp"

#include <boost/geometry/geometries/multi_point.hpp>

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::multi_point<point_type>  multi_point_type;



//===========================================================================
//===========================================================================
//===========================================================================


BOOST_AUTO_TEST_CASE( test_union_point_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / POINT UNION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, P, MP, bg::overlay_union
        > tester;

    tester::apply
        ("ppu01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,1 1)")
         );

    tester::apply
        ("ppu02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );
}


BOOST_AUTO_TEST_CASE( test_union_multipoint_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / POINT UNION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, P, MP, bg::overlay_union
        > tester;

    tester::apply
        ("mppu01",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,1 1)")
         );

    tester::apply
        ("mppu02",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("mppu03",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 1)")
         );

    tester::apply
        ("mppu04",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("mppu05",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0,1 1)")
         );

    tester::apply
        ("mppu06",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<P>("POINT(1 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)")
         );

    tester::apply
        ("mppu07",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,1 0)")
         );

    tester::apply
        ("mppu08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );
}


BOOST_AUTO_TEST_CASE( test_union_multipoint_multipoint )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / MULTIPOINT UNION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, MP, MP, bg::overlay_union
        > tester;

    tester::apply
        ("mpmpu01",
         from_wkt<MP>("MULTIPOINT(2 2,3 3,0 0,0 0,2 2,1 1,1 1,1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,1 1,1 1,1 1)"),
         from_wkt<MP>("MULTIPOINT(2 2,3 3,0 0,0 0,2 2,1 1,1 1,1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,1 1,1 1,1 1,2 2,3 3,0 0,0 0,2 2)")
         );

    tester::apply
        ("mpmpu02",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)")
         );

    tester::apply
        ("mpmpu03",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)")
         );

    tester::apply
        ("mpmpu04",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)")
         );

    tester::apply
        ("mpmpu05",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmpu06",
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0,3 0,0 0,1 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(0 1,0 2,1 0,0 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0,3 0,0 0,1 0,2 0,0 1,0 2)"),
         from_wkt<MP>("MULTIPOINT(0 1,0 2,1 0,0 0,2 0,3 0)")
         );
}
