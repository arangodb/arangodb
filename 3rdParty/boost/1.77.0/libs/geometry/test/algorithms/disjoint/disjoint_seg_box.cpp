// Boost.Geometry

// Copyright (c) 2016-2019 Oracle and/or its affiliates.

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

    //https://github.com/boostorg/geometry/issues/579
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(10 10,20 20)",
                                                             "SEGMENT(12 2,12 1)",
                                                             true);
    test_disjoint<bg::model::box<P>, bg::model::segment<P> >("BOX(10 10,20 20)",
                                                             "SEGMENT(12 1,12 2)",
                                                             true);

    //based on https://github.com/boostorg/geometry/issues/851
    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(13.381347 50.625, 13.40332 50.646972)",
      "SEGMENT(9.18 48.78,13.4 52.52)",
      true);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(11.22802734375 50.972264889367494, 11.359863281250002 51.06211251399776)",
      "SEGMENT(9.18 48.78,13.4 52.52)",
      true);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(10.9423828125 50.527396813293024, 12.06298828125 50.65294336725708)",
      "SEGMENT(9.18 48.78,13.4 52.52)",
      false);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(13.238525390625 52.49448734004673, 13.3319091796875 52.549636074382306)",
      "SEGMENT(9.18 48.78,13.4 52.52)",
      true);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(13.40332 -50.646972, 13.381347 -50.625)",
      "SEGMENT(9.18 -48.78,13.4 -52.52)",
      true);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(11.359863281250002 -51.06211251399776,11.22802734375 -50.972264889367494)",
      "SEGMENT(9.18 -48.78,13.4 -52.52)",
      true);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(0 1,1 2)",
      "SEGMENT(0 0,0 3)",
      false);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(0 1,1 2)",
      "SEGMENT(1 0,1 3)",
      false);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(1 0,2 1)",
      "SEGMENT(0 0,3 0)",
      false);

    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(1 0,2 1)",
      "SEGMENT(0 1,3 1)",
      true);

    // segment on the equator
    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(1 1,2 2)",
      "SEGMENT(0 0,3 0)",
      true);
    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(2 -2,1 -1)",
      "SEGMENT(0 0,3 0)",
      true);
    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(1 -1,2 1)",
      "SEGMENT(0 0,3 0)",
      false);
    test_disjoint
    <
        bg::model::box<P>,
        bg::model::segment<P>
    >("BOX(4 -1,5 1)",
      "SEGMENT(0 0,3 0)",
      true);
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

    return 0;
}
