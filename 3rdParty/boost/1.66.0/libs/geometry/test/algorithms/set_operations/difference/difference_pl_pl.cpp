// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_difference_pointlike_pointlike
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


BOOST_AUTO_TEST_CASE( test_difference_point_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / POINT DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, P, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("ppdf01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("ppdf02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_multipoint_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / POINT DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, P, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("mppdf01",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("mppdf02",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mppdf03",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("mppdf04",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mppdf05",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<P>("POINT(1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("mppdf06",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<P>("POINT(1 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mppdf07",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(1 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mppdf08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_point_multipoint )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / MULTIPOINT DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, MP, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("pmpdf01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,1 1,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,1 1,1 1)")
         );

    tester::apply
        ("pmpdf02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(1 0,1 1)")
         );

    tester::apply
        ("pmpdf03",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_multipoint_multipoint )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / MULTIPOINT DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, MP, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("mpmpdf01",
         from_wkt<MP>("MULTIPOINT(2 2,3 3,0 0,0 0,2 2,1 1,1 1,1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(1 0,1 1,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT(2 2,3 3,0 0,0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT(4 4)")
         );

    tester::apply
        ("mpmpdf02",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmpdf03",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,0 0)")
         );

    tester::apply
        ("mpmpdf04",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmpdf05",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<MP>("MULTIPOINT()")
         );
}
