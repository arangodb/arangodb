// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2013-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include "test_difference.hpp"


template <typename P, bool ClockWise, bool Closed>
void test_spikes_in_ticket_8364()
{
    ut_settings ignore_validity;
    ignore_validity.test_validity = false;

    // See: https://svn.boost.org/trac/boost/ticket/8364
    //_TPolygon<T> polygon( "MULTIPOLYGON(((1031 1056,3232 1056,3232 2856,1031 2856)))" );
    //polygon -= _TPolygon<T>( "MULTIPOLYGON(((1032 1458,1032 1212,2136 2328,3234 2220,3234 2412,2136 2646)))" );
    //polygon -= _TPolygon<T>( "MULTIPOLYGON(((1032 1764,1032 1458,2136 2646,3234 2412,3234 2532,2136 2790)))" );
    // RESULTS OF ABOVE IS USED IN STEP 3 BELOW
    //polygon -= _TPolygon<T>( "MULTIPOLYGON(((1032 2130,1032 1764,2052 2712)),((3234 2580,2558 2690,3234 2532)),((2558 2690,2136 2790,2052 2712,2136 2760)))" ); USED IN STEP 3
    //polygon -= _TPolygon<T>( "MULTIPOLYGON(((1032 2556,1032 2130,1778 2556)),((3234 2580,2136 2760,1778 2556,3234 2556)))" ); USED IN STEP 4
    // NOTE: polygons below are closed and clockwise

    typedef typename bg::coordinate_type<P>::type ct;
    typedef bg::model::polygon<P, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    // The difference of polygons below result in a spike. The spike should be there, it is also generated in ttmath,
    // and (e.g.) in SQL Server. However, using int-coordinates, the spike makes the polygon invalid. Therefore it is now (since August 2013) checked and removed.

#ifdef BOOST_GEOMETRY_TEST_INCLUDE_FAILING_TESTS
    // TODO: commented working at ii/validity, this changes the area slightly, to be checked
    // So using int's, the spike is removed automatically. Therefore there is one polygon less, and less points. Also area differs
    test_one<polygon, multi_polygon, multi_polygon>("ticket_8364_step3",
        "MULTIPOLYGON(((3232 2532,2136 2790,1032 1764,1032 1458,1032 1212,2136 2328,3232 2220,3232 1056,1031 1056,1031 2856,3232 2856,3232 2532)))",
        "MULTIPOLYGON(((1032 2130,2052 2712,1032 1764,1032 2130)),((3234 2580,3234 2532,2558 2690,3234 2580)),((2558 2690,2136 2760,2052 2712,2136 2790,2558 2690)))",
        2,
        if_typed<ct, int>(19, 22),
        if_typed<ct, int>(2775595.5, 2775256.487954), // SQL Server: 2775256.47588724
        3,
        -1, // don't check point-count
        if_typed<ct, int>(7893.0, 7810.487954), // SQL Server: 7810.48711165739
        if_typed<ct, int>(1, 5),
        -1,
        if_typed<ct, int>(2783349.5, 2775256.487954 + 7810.487954),
        ignore_validity);
#endif

    test_one<polygon, multi_polygon, multi_polygon>("ticket_8364_step4",
        "MULTIPOLYGON(((2567 2688,2136 2790,2052 2712,1032 2130,1032 1764,1032 1458,1032 1212,2136 2328,3232 2220,3232 1056,1031 1056,1031 2856,3232 2856,3232 2580,2567 2688)))",
        "MULTIPOLYGON(((1032 2556,1778 2556,1032 2130,1032 2556)),((3234 2580,3234 2556,1778 2556,2136 2760,3234 2580)))",
        1,
        if_typed<ct, int>(20, 20),
        if_typed<ct, int>(2615783.5, 2616029.559567), // SQL Server: 2616029.55616044
        1,
        if_typed<ct, int>(9, 11),
        if_typed<ct, int>(161133.5, 161054.559567), // SQL Server: 161054.560110092
        if_typed<ct, int>(1, 2),
        if_typed<ct, int>(28, 31),
        if_typed<ct, int>(2776875.5, 2616029.559567 + 161054.559567),
        ignore_validity);
}

template <typename P, bool ClockWise, bool Closed>
void test_spikes_in_ticket_8365()
{
    // See: https://svn.boost.org/trac/boost/ticket/8365
    // NOTE: polygons below are closed and clockwise

    typedef typename bg::coordinate_type<P>::type ct;
    typedef bg::model::polygon<P, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    test_one<polygon, multi_polygon, multi_polygon>("ticket_8365_step2",
        "MULTIPOLYGON(((971 2704,971 1402,4640 1402,3912 1722,3180 2376,3912 1884,4643 1402,5395 1402,5395 3353,971 3353,971 2865,1704 3348)))",
        "MULTIPOLYGON(((5388 1560,4650 1722,3912 1884,4650 1398)),((2442 3186,1704 3348,966 2700,1704 3024)))",
        if_typed<ct, int>(2, 2),
        if_typed<ct, int>(21, 21),
        if_typed<ct, int>(7975092.5, 7975207.6047877), // SQL Server:
        2,
        -1,
        if_typed<ct, int>(196.5, 197.1047877), // SQL Server:
        if_typed<ct, int>(3, 4),
        -1,
        if_typed<ct, int>(7975092.5 + 196.5, 7975207.6047877 + 197.1047877));
}





int test_main(int, char* [])
{
    test_spikes_in_ticket_8364<bg::model::d2::point_xy<double>, true, true>();
    test_spikes_in_ticket_8364<bg::model::d2::point_xy<double>, false, false>();
    test_spikes_in_ticket_8365<bg::model::d2::point_xy<double>, true, true>();
    test_spikes_in_ticket_8365<bg::model::d2::point_xy<double>, false, false>();

    test_spikes_in_ticket_8364<bg::model::d2::point_xy<int>, true, true>();
    test_spikes_in_ticket_8364<bg::model::d2::point_xy<int>, false, false>();
    test_spikes_in_ticket_8365<bg::model::d2::point_xy<int>, true, true >();
    test_spikes_in_ticket_8365<bg::model::d2::point_xy<int>, false, false >();

#ifdef HAVE_TTMATH
    std::cout << "Testing TTMATH" << std::endl;
    test_spikes_in_ticket_8364<bg::model::d2::point_xy<ttmath_big>, true, true>();
    test_spikes_in_ticket_8365<bg::model::d2::point_xy<ttmath_big>, true, true>();
#endif

    return 0;
}

