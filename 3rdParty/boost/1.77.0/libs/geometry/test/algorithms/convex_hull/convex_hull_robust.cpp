// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <string>

#include "test_convex_hull.hpp"

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/multiprecision/cpp_dec_float.hpp>

template <typename CT, typename CTmp>
void test_all()
{
    typedef bg::model::d2::point_xy<CT> P;
    typedef bg::model::d2::point_xy<CTmp> Pmp;

    // from sample polygon, with concavity
    auto polygon_wkt0 = "polygon((2.0 1.3, 2.4 1.7, 2.8 1.8, 3.4 1.2, 3.7 1.6,\
        3.4 2.0, 4.1 3.0, 5.3 2.6, 5.4 1.2, 4.9 0.8, 2.9 0.7,2.0 1.3))";
    test_geometry<bg::model::polygon<P>, robust_cartesian>(
        polygon_wkt0, 12, 8, 5.245);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_sbt>(
        polygon_wkt0, 12, 8, 5.245);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_fast>(
        polygon_wkt0, 12, 8, 5.245);
    test_geometry<bg::model::polygon<Pmp>, non_robust_cartesian_sbt>(
        polygon_wkt0, 12, 8, 5.245);

    // some colinear cases are ok with non robust predicate
    auto polygon_wkt1 = "polygon((0.50000000000001621 0.50000000000001243,\
        7.3000000000000173 7.3000000000000167,\
        24.00000000000005 24.000000000000053,\
        24.000000000000068 24.000000000000071,\
        0 1,8 4,25 26,26 25,19 11))";
    test_geometry<bg::model::polygon<P>, robust_cartesian >(
        polygon_wkt1, 9, 7, 137.25);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_sbt >(
        polygon_wkt1, 9, 7, 137.25);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_fast >(
        polygon_wkt1, 9, 7, 137.25);
    test_geometry<bg::model::polygon<Pmp>, non_robust_cartesian_sbt >(
        polygon_wkt1, 9, 7, 137.25);

    // slightly non-convex result but areas equal
    auto polygon_wkt2 = "polygon((27.643564356435643 -21.881188118811881,\
        83.366336633663366 15.544554455445542,\
        4.0 4.0,\
        73.415841584158414 8.8613861386138595,\
        27.643564356435643 -21.881188118811881))";
    test_geometry<bg::model::polygon<P>, robust_cartesian >(
        polygon_wkt2, 5, 4, 1163.5247524752476);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_sbt >(
        polygon_wkt2, 5, 5, 1163.5247524752476);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_fast >(
        polygon_wkt2, 5, 4, 1163.5247524752476);
    test_geometry<bg::model::polygon<Pmp>, non_robust_cartesian_sbt >(
        polygon_wkt2, 5, 4, 1163.5247524752476);

    // wrong orientation / sign in area
    // here non_robust_cartesian_sbt computes wrong result with mp arithmetic
    // correct results double checked with CGAL 5.1
    auto polygon_wkt3 = "polygon((200.0 49.200000000000003,\
        100.0 49.600000000000001,\
        -233.33333333333334 50.93333333333333,\
        166.66666666666669 49.333333333333336,\
        200.0 49.200000000000003))";
    test_geometry<bg::model::polygon<P>, robust_cartesian, precise_cartesian >(
        polygon_wkt3, 5, 4, 1.4210854715202004e-14);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_sbt, precise_cartesian >(
        polygon_wkt3, 5, 4, -1.4210854715202004e-14);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_fast, precise_cartesian >(
        polygon_wkt3, 5, 4, 1.4210854715202004e-14);
    test_geometry<bg::model::polygon<Pmp>, non_robust_cartesian_sbt >(
        polygon_wkt3, 5, 5, 1.69333333333333265e-13);

    // missing one point could lead in arbitrary large errors in area
    auto polygon_wkt4 = "polygon((0.10000000000000001 0.10000000000000001,\
        0.20000000000000001 0.20000000000000004,\
        0.79999999999999993 0.80000000000000004,\
        1.267650600228229e30 1.2676506002282291e30,\
        0.10000000000000001 0.10000000000000001))";
    test_geometry<bg::model::polygon<P>, robust_cartesian, precise_cartesian>(
        polygon_wkt4, 5, 5, -0.315);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_sbt, precise_cartesian>(
        polygon_wkt4, 5, 4, 0);
    test_geometry<bg::model::polygon<P>, non_robust_cartesian_fast, precise_cartesian>(
        polygon_wkt4, 5, 4, -0.015);
    test_geometry<bg::model::polygon<Pmp>, non_robust_cartesian_sbt>(
        polygon_wkt4, 5, 5, 3.472078301e+13);
}


int test_main(int, char* [])
{
    using boost::multiprecision::cpp_dec_float_50;
    test_all<double, cpp_dec_float_50>();

    return 0;
}
