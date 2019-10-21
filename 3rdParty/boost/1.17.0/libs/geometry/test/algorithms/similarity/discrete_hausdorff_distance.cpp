// Boost.Geometry

// Copyright (c) 2018 Yaghyavardhan Singh Khangarot, Hyderabad, India.

// Contributed and/or modified by Yaghyavardhan Singh Khangarot,
//   as part of Google Summer of Code 2018 program.

// This file was modified by Oracle on 2018.
// Modifications copyright (c) 2018 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <vector>

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_point.hpp>

#include "test_hausdorff_distance.hpp"

    template <typename P>
void test_all_cartesian()
{
    typedef bg::model::linestring<P> linestring_2d;
    typedef bg::model::multi_linestring<linestring_2d> mlinestring_t;
    typedef bg::model::multi_point<P> mpoint_t;

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    typedef typename coordinate_system<P>::type CordType;
    std::cout << typeid(CordType).name() << std::endl;
#endif

    test_geometry<P,mpoint_t>("POINT(3 1)","MULTIPOINT(0 0,3 4,4 3)", sqrt(5.0));
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)", 3);
    test_geometry<mpoint_t,mpoint_t>("MULTIPOINT(3 0,2 1,3 2)","MULTIPOINT(0 0,3 4,4 3)", 3);
    test_geometry<linestring_2d,mlinestring_t >("LINESTRING(1 1,2 2,4 3)","MULTILINESTRING((0 0,3 4,4 3),(1 1,2 2,4 3))", sqrt(5.0));
    test_geometry<mlinestring_t,mlinestring_t >("MULTILINESTRING((3 0,2 1,3 2),(0 0,3 4,4 3))","MULTILINESTRING((0 0,3 4,4 3),(3 0,2 1,3 2))", 3);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(1 1, 0 1, 0 0, 1 0, 1 1)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(1 1, 0 0, 1 0, 1 1)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0)","LINESTRING(0 0, 1 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0,3 4,4 3)","LINESTRING(4 3,3 4,0 0)",0);
}

    template <typename P>
void test_all_geographic()
{
    typedef bg::model::linestring<P> linestring_2d;
    typedef bg::model::multi_linestring<linestring_2d> mlinestring_t;
    typedef bg::model::multi_point<P> mpoint_t;
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    typedef typename coordinate_system<P>::type CordType;
    std::cout << typeid(CordType).name() << std::endl;
#endif

    test_geometry<P,mpoint_t>("POINT(3 1)","MULTIPOINT(0 0,3 4,4 3)", 247552);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)", 333958);
    test_geometry<mpoint_t,mpoint_t>("MULTIPOINT(3 0,2 1,3 2)","MULTIPOINT(0 0,3 4,4 3)", 333958);
    test_geometry<linestring_2d,mlinestring_t >("LINESTRING(1 1,2 2,4 3)","MULTILINESTRING((0 0,3 4,4 3),(1 1,2 2,4 3))", 247518);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)",bg::strategy::distance::geographic<bg::strategy::vincenty>(), 333958.472379679);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)",bg::strategy::distance::geographic<bg::strategy::thomas>(), 333958.472379679);
    test_geometry<mlinestring_t,mlinestring_t >("MULTILINESTRING((3 0,2 1,3 2),(0 0,3 4,4 3))","MULTILINESTRING((0 0,3 4,4 3),(3 0,2 1,3 2))",333958);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(1 1, 0 1, 0 0, 1 0, 1 1)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(1 1, 0 0, 1 0, 1 1)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0)","LINESTRING(0 0, 1 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0,3 4,4 3)","LINESTRING(4 3,3 4,0 0)",0);
}

    template <typename P>
void test_all_spherical_equ()
{
    typedef bg::model::linestring<P> linestring_2d;
    typedef bg::model::multi_linestring<linestring_2d> mlinestring_t;
    typedef bg::model::multi_point<P> mpoint_t;

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    typedef typename coordinate_system<P>::type CordType;
    std::cout << typeid(CordType).name() << std::endl;
#endif

    test_geometry<P,mpoint_t>("POINT(3 1)","MULTIPOINT(0 0,3 4,4 3)", 0.03902);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)", 0.05236);
    test_geometry<mpoint_t,mpoint_t>("MULTIPOINT(3 0,2 1,3 2)","MULTIPOINT(0 0,3 4,4 3)", 0.05236);
    test_geometry<linestring_2d,mlinestring_t >("LINESTRING(1 1,2 2,4 3)","MULTILINESTRING((0 0,3 4,4 3),(1 1,2 2,4 3))", 0.03900);
    test_geometry<mlinestring_t,mlinestring_t >("MULTILINESTRING((3 0,2 1,3 2),(0 0,3 4,4 3))","MULTILINESTRING((0 0,3 4,4 3),(3 0,2 1,3 2))",0.05236);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(1 1, 0 1, 0 0, 1 0, 1 1)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(1 1, 0 0, 1 0, 1 1)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0)","LINESTRING(0 0, 1 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0,3 4,4 3)","LINESTRING(4 3,3 4,0 0)",0);
}

int test_main(int, char* [])
{
    //Cartesian Coordinate System
    test_all_cartesian<bg::model::d2::point_xy<int,bg::cs::cartesian> >();
    test_all_cartesian<bg::model::d2::point_xy<float,bg::cs::cartesian> >();
    test_all_cartesian<bg::model::d2::point_xy<double,bg::cs::cartesian> >();

    //Geographic Coordinate System
    test_all_geographic<bg::model::d2::point_xy<float,bg::cs::geographic<bg::degree> > >();
    test_all_geographic<bg::model::d2::point_xy<double,bg::cs::geographic<bg::degree> > >();

    //Spherical_Equatorial Coordinate System
    test_all_spherical_equ<bg::model::d2::point_xy<float,bg::cs::spherical_equatorial<bg::degree> > >();
    test_all_spherical_equ<bg::model::d2::point_xy<double,bg::cs::spherical_equatorial<bg::degree> > >();

    return 0;
}
