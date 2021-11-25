// Boost.Geometry

// Copyright (c) 2018 Yaghyavardhan Singh Khangarot, Hyderabad, India.

// Contributed and/or modified by Yaghyavardhan Singh Khangarot, as part of Google Summer of Code 2018 program.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <vector>

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include "test_frechet_distance.hpp"

    template <typename P>
void test_all_cartesian()
{
    typedef bg::model::linestring<P> linestring_2d;
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    typedef typename coordinate_system<P>::type CordType;
    std::cout << typeid(CordType).name() << std::endl;
#endif

    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)", 3);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)",bg::strategy::distance::pythagoras<>(), 3);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(1 1, 0 1, 0 0, 1 0, 1 1)",sqrt(2.0));
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(1 1, 0 0, 1 0, 1 1)",sqrt(2.0));
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0)","LINESTRING(0 0, 1 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0,3 4,4 3)","LINESTRING(4 3,3 4,0 0)",5);


}

    template <typename P>
void test_all_geographic()
{
    typedef bg::model::linestring<P> linestring_2d;

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    typedef typename coordinate_system<P>::type CordType;
    std::cout << typeid(CordType).name() << std::endl;
#endif

    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)", 333958);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)",bg::strategy::distance::geographic<bg::strategy::vincenty>(), 333958.472379679);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)",bg::strategy::distance::geographic<bg::strategy::thomas>(), 333958.472379679);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(1 1, 0 1, 0 0, 1 0, 1 1)",156898);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(1 1, 0 0, 1 0, 1 1)",156898);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0)","LINESTRING(0 0, 1 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0,3 4,4 3)","LINESTRING(4 3,3 4,0 0)",555093);

}

    template <typename P>
void test_all_spherical_equ()
{
    typedef bg::model::linestring<P> linestring_2d;

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    typedef typename coordinate_system<P>::type CordType;
    std::cout << typeid(CordType).name() << std::endl;
#endif

    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)", 0.05235987);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(3 0,2 1,3 2)","LINESTRING(0 0,3 4,4 3)",bg::strategy::distance::haversine<int>(), 0.05235987);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 1, 0 0)","LINESTRING(1 1, 0 1, 0 0, 1 0, 1 1)",0.02468205);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(0 0, 1 0, 1 1, 0 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0, 1 1, 0 0)","LINESTRING(1 1, 0 0, 1 0, 1 1)",0.02468205);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0, 1 0)","LINESTRING(0 0, 1 0)",0);
    test_geometry<linestring_2d,linestring_2d >("LINESTRING(0 0,3 4,4 3)","LINESTRING(4 3,3 4,0 0)",0.0872409);

}

int test_main(int, char* [])
{
    //Cartesian Coordinate System
    test_all_cartesian<bg::model::d2::point_xy<double,bg::cs::cartesian> >();

    //Geographic Coordinate System
    test_all_geographic<bg::model::d2::point_xy<double,bg::cs::geographic<bg::degree> > >();

    //Spherical_Equatorial Coordinate System
    test_all_spherical_equ<bg::model::d2::point_xy<double,bg::cs::spherical_equatorial<bg::degree> > >();

    return 0;
}
