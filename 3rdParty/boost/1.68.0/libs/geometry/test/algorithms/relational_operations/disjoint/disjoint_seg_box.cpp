// Boost.Geometry

// Copyright (c) 2016-2017 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <test_common/test_point.hpp>

#include <boost/geometry/formulas/andoyer_inverse.hpp>
#include <boost/geometry/formulas/thomas_inverse.hpp>
#include <boost/geometry/formulas/vincenty_inverse.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/algorithms/disjoint.hpp>

#include <geometry_test_common.hpp>

#include "test_disjoint_seg_box.hpp"


namespace bg = boost::geometry;

//Tests for disjoint(point, box), disjoint(box, box) and disjoint(segment, box)

template <typename P>
void disjoint_tests_1()
{
    test_disjoint<bg::model::box<P>, P>("BOX(1 1,3 3)", "POINT(4 4)", true);
    test_disjoint<bg::model::box<P>, P>("BOX(1 1,3 3)", "POINT(2 2)", false);
    test_disjoint<bg::model::box<P>, P>("BOX(1 1,3 3)", "POINT(3 3)", false);
    test_disjoint<bg::model::box<P>, P>("BOX(1 1,3 3)", "POINT(2 3)", false);

    test_disjoint<bg::model::box<P>, bg::model::box<P> >("BOX(1 1,3 3)",
                                                         "BOX(1 4,5 5)",
                                                         true);
    test_disjoint<bg::model::box<P>, bg::model::box<P> >("BOX(1 1,3 3)",
                                                         "BOX(2 2,4 4)",
                                                         false);
    test_disjoint<bg::model::box<P>, bg::model::box<P> >("BOX(1 1,3 3)",
                                                         "BOX(3 3,4 4)",
                                                         false);
    test_disjoint<bg::model::box<P>, bg::model::box<P> >("BOX(1 1,3 3)",
                                                         "BOX(2 3,4 4)",
                                                         false);

    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(1 4, 5 5)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(3 3, 5 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(1 1, 4 1)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(1 2, 5 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(1 2, 3 2)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(0 0, 4 0)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(2 2, 4 4)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(4 4, 2 2)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(1.5 1.5, 2 2)",
                                                             false);
}

template <typename P>
void disjoint_tests_2(bool expected_result)
{
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(1 0.999, 10 0.999)",
                                                             expected_result);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(10 0.999, 1 0.999)",
                                                             expected_result);
}

template <typename P>
void disjoint_tests_3(bool expected_result)
{
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(3 4.42, 100 5)",
                                                             "SEGMENT(2 2.9, 100 2.9)",
                                                             expected_result);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(3 4.42, 100 5)",
                                                             "SEGMENT(100 2.9, 2 2.9)",
                                                             expected_result);
}

template <typename P>
void disjoint_tests_4(bool expected_result)
{
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(0 0.99999999, 2 0.99999999)",
                                                             expected_result);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(1 1,3 3)",
                                                             "SEGMENT(2 0.99999999, 0 0.99999999)",
                                                             expected_result);
}

template <typename P, typename CT>
void disjoint_tests_with_strategy(bool expected_result)
{
    bg::strategy::disjoint::segment_box_geographic
    <
        bg::strategy::andoyer,
        bg::srs::spheroid<CT>,
        CT
    > geographic_andoyer;

    bg::strategy::disjoint::segment_box_geographic
    <
        bg::strategy::thomas,
        bg::srs::spheroid<CT>,
        CT
    > geographic_thomas;

    bg::strategy::disjoint::segment_box_geographic
    <
        bg::strategy::vincenty,
        bg::srs::spheroid<CT>,
        CT
    > geographic_vincenty;

    test_disjoint_strategy<bg::model::box<P>, bg::model::segment<P> >
            ("BOX(1 1,3 3)", "SEGMENT(1 0.999, 10 0.999)",
             expected_result, geographic_andoyer);
    test_disjoint_strategy<bg::model::box<P>, bg::model::segment<P> >
            ("BOX(1 1,3 3)", "SEGMENT(1 0.999, 10 0.999)",
             expected_result, geographic_thomas);
    test_disjoint_strategy<bg::model::box<P>, bg::model::segment<P> >
            ("BOX(1 1,3 3)", "SEGMENT(1 0.999, 10 0.999)",
             expected_result, geographic_vincenty);
}

template <typename P>
void disjoint_tests_sph_geo()
{
    //Case A: box intersects without containing the vertex
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 6, 120 7)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 -6, 120 -7)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             false);

    //Case B: box intersects and contains the vertex
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 9, 120 10)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 -10, 120 -9)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             false);

    //Case C: bounding boxes disjoint
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 10, 10 20)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 -20, 10 -10)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             true);

    //Case D: bounding boxes intersect but box segment are disjoint
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 9, 0.1 20)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 -20, 0.1 -9)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             true);

    //Case E: geodesic intersects box but box segment are disjoint
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(121 0, 122 10)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(121 -10, 122 0)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             true);

    //Case F: segment crosses box
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(100 0, 110 20)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(100 -20, 110 0)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             false);

    //Case G: box contains one segment endpoint
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(110 0, 130 10)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(110 -10, 130 0)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             false);

    //Case H: box below segment
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(50 0, 70 6)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(50 -6, 70 0)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             true);

    //Case I: box contains segment
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(-10 0, 130 10)",
                                                             "SEGMENT(0 5, 120 5)",
                                                             false);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(-10 -10, 130 0)",
                                                             "SEGMENT(0 -5, 120 -5)",
                                                             false);

    //ascending segment
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 10, 120 10.1)",
                                                             "SEGMENT(0 5, 120 5.1)",
                                                             false);

    //descending segment
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 9.8, 120 10)",
                                                             "SEGMENT(0 5, 120 4.9)",
                                                             false);

    //ascending segment both hemispheres
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(100 5, 120 6)",
                                                             "SEGMENT(0 -1, 120 4.9)",
                                                             false);

    //descending segment both hemispheres
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(0 5, 20 6)",
                                                             "SEGMENT(0 4.9, 120 -1)",
                                                             false);
}

template <typename CT>
void test_all()
{
    typedef bg::model::d2::point_xy<CT> point;
    typedef bg::model::point<CT, 2,
            bg::cs::spherical_equatorial<bg::degree> > sph_point;
    typedef bg::model::point<CT, 2,
            bg::cs::geographic<bg::degree> > geo_point;

    disjoint_tests_1<point>();
    disjoint_tests_1<sph_point>();
    disjoint_tests_1<geo_point>();

    disjoint_tests_2<point>(true);
    disjoint_tests_2<sph_point>(false);
    disjoint_tests_2<geo_point>(false);

    //illustrate difference between spherical and geographic computation on same data
    disjoint_tests_3<point>(true);
    disjoint_tests_3<sph_point>(true);
    disjoint_tests_3<geo_point>(false);

    disjoint_tests_4<sph_point>(false);
    disjoint_tests_4<geo_point>(false);

    disjoint_tests_with_strategy<geo_point, CT>(false);

    disjoint_tests_sph_geo<sph_point>();
    disjoint_tests_sph_geo<geo_point>();
}


int test_main( int , char* [] )
{
    test_all<float>();
    test_all<double>();

#ifdef HAVE_TTMATH
    common_tests<bg::model::d2::point_xy<ttmath_big> >();
    common_tests<bg::model::point<ttmath_big, 3, bg::cs::cartesian> >();
#endif

    return 0;
}
