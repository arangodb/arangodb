// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014, 2015.
// Modifications copyright (c) 2014-2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithms/test_centroid.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>

#include <test_geometries/all_custom_polygon.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)

template <typename Polygon>
void test_polygon()
{
    test_centroid<Polygon>(
        "POLYGON((2 1.3,2.4 1.7,2.8 1.8,3.4 1.2"
        ",3.7 1.6,3.4 2,4.1 3,5.3 2.6,5.4 1.2,4.9 0.8,2.9 0.7,2 1.3))",
        4.06923363095238, 1.65055803571429);

    // with holes
    test_centroid<Polygon>(
        "POLYGON((2 1.3,2.4 1.7,2.8 1.8,3.4 1.2"
        ",3.7 1.6,3.4 2,4.1 3,5.3 2.6,5.4 1.2,4.9 0.8,2.9 0.7,2 1.3)"
        ",(4 2,4.2 1.4,4.8 1.9,4.4 2.2,4 2))",
        4.0466264962959677, 1.6348996057331333);

    test_centroid<Polygon>("POLYGON((0 0,0 10,10 10,10 0,0 0))", 5.0, 5.0);
    test_centroid<Polygon>("POLYGON((-10 0,0 0,0 -10,-10 -10,-10 0))", -5.0, -5.0);

    // invalid, self-intersecting polygon (area = 0)
    test_centroid<Polygon>("POLYGON((1 1,4 -2,4 2,10 0,1 0,10 1,1 1))", 1.0, 1.0);
    // invalid, degenerated
    test_centroid<Polygon>("POLYGON((1 1,1 1,1 1,1 1))", 1.0, 1.0);
    test_centroid<Polygon>("POLYGON((1 1))", 1.0, 1.0);

    // should (1.5 1) be returned?
    // if yes, then all other Polygons degenerated to Linestrings should be handled
    test_centroid<Polygon>("POLYGON((1 1,2 1,1 1,1 1))", 1.0, 1.0);

    // reported 2015.04.24
    // input INT, result FP
    test_centroid
        <
            bg::model::polygon<bg::model::d2::point_xy<int> >,
            typename bg::point_type<Polygon>::type,
            typename bg::coordinate_type<Polygon>::type
        >("POLYGON((1 1, 1 2, 2 2, 2 1, 1 1))", 1.5, 1.5);
}


template <typename P>
void test_2d()
{
    test_centroid<bg::model::linestring<P> >("LINESTRING(1 1, 2 2, 3 3)", 2.0, 2.0);
    test_centroid<bg::model::linestring<P> >("LINESTRING(0 0,0 4, 4 4)", 1.0, 3.0);
    test_centroid<bg::model::linestring<P> >("LINESTRING(0 0,3 3,0 6,3 9,0 12)", 1.5, 6.0);

    test_centroid<bg::model::linestring<P> >("LINESTRING(1 1,10 1,1 0,10 0,4 -2,1 1)",
                                             5.41385255923004, 0.13507358481085);

    // degenerated linestring (length = 0)
    test_centroid<bg::model::linestring<P> >("LINESTRING(1 1, 1 1)", 1.0, 1.0);
    test_centroid<bg::model::linestring<P> >("LINESTRING(1 1)", 1.0, 1.0);

    {
        bg::model::linestring<P> ls;
        // LINESTRING(1 -1,1e308 -1e308,0.0001 0.000)
        bg::append(ls, P(1, -1));
        typedef typename bg::coordinate_type<P>::type coord_type;
        //double m = 1.0e308;
        coord_type m = (std::numeric_limits<coord_type>::max)();
        bg::append(ls, P(coord_type(m), coord_type(-m)));
        bg::append(ls, P(coord_type(0.0001), coord_type(0.000)));
        if (BOOST_GEOMETRY_CONDITION((boost::is_same<typename bg::coordinate_type<P>::type, double>::value)))
        {
            // for doubles the INF is detected and the calculation stopped
            // currently for Geometries for which the centroid can't be calculated
            // the first Point is returned
            test_centroid<bg::model::linestring<P> >(ls, 1.0, -1.0);
        }
        else
        {
            // for floats internally the double is used to store intermediate results
            // this type is capable to store MAX_FLT and "correctly" calculate the centroid
            // test_centroid<bg::model::linestring<P> >(ls, m/3, -m/3);
            // the result is around (1.7e38 -1.7e38)
        }
    }

    test_centroid<bg::model::segment<P> >("LINESTRING(1 1, 3 3)", 2.0, 2.0);

    test_centroid<bg::model::ring<P> >(
        "POLYGON((2 1.3,2.4 1.7,2.8 1.8,3.4 1.2"
        ",3.7 1.6,3.4 2,4.1 3,5.3 2.6,5.4 1.2,4.9 0.8,2.9 0.7,2 1.3))",
        4.06923363095238, 1.65055803571429);

    test_polygon<bg::model::polygon<P> >();
    test_polygon<all_custom_polygon<P> >();

    // ccw
    test_centroid<bg::model::ring<P, false> >(
        "POLYGON((2 1.3,2.9 0.7,4.9 0.8,5.4 1.2,5.3 2.6,4.1 3,3.4 2"
            ",3.7 1.6,3.4 1.2,2.8 1.8,2.4 1.7,2 1.3))",
        4.06923363095238, 1.65055803571429);

    // open / closed
    test_centroid<bg::model::ring<P, true, true> >(
            "POLYGON((1 1,2 2,3 1,2 0,1 1))", 2.0, 1.0);
    test_centroid<bg::model::ring<P, true, false> >(
            "POLYGON((1 1,2 2,3 1,2 0))", 2.0, 1.0);

    test_centroid<bg::model::box<P> >("POLYGON((1 2,3 4))", 2, 3);
    test_centroid<P>("POINT(3 3)", 3, 3);

    // INT -> FP
    test_centroid
        <
            bg::model::ring<bg::model::d2::point_xy<int> >,
            P, typename bg::coordinate_type<P>::type
        >("POLYGON((1 1, 1 2, 2 2, 2 1, 1 1))", 1.5, 1.5);
    test_centroid
        <
            bg::model::linestring<bg::model::d2::point_xy<int> >,
            P, typename bg::coordinate_type<P>::type
        >("LINESTRING(1 1, 2 2)", 1.5, 1.5);
    test_centroid
        <
            bg::model::box<bg::model::d2::point_xy<int> >,
            P, typename bg::coordinate_type<P>::type
        >("BOX(1 1, 2 2)", 1.5, 1.5);
}


template <typename P>
void test_3d()
{
    test_centroid<bg::model::linestring<P> >("LINESTRING(1 2 3,4 5 -6,7 -8 9,-10 11 12,13 -14 -15, 16 17 18)",
                                             5.6748865168734692, 0.31974938587214002, 1.9915270387763671);
    test_centroid<bg::model::box<P> >("POLYGON((1 2 3,5 6 7))", 3, 4, 5);
    test_centroid<bg::model::segment<P> >("LINESTRING(1 1 1,3 3 3)", 2, 2, 2);
    test_centroid<P>("POINT(1 2 3)", 1, 2, 3);
}


template <typename P>
void test_5d()
{
    test_centroid<bg::model::linestring<P> >("LINESTRING(1 2 3 4 95,4 5 -6 24 40,7 -8 9 -5 -7,-10 11 12 -5 5,13 -14 -15 4 3, 16 17 18 5 12)",
                                             4.9202312983547678, 0.69590937869808345, 1.2632138719797417, 6.0468332057401986, 23.082402715244868);
}

template <typename P>
void test_exceptions()
{
    test_centroid_exception<bg::model::linestring<P> >();
    test_centroid_exception<bg::model::polygon<P> >();
    test_centroid_exception<bg::model::ring<P> >();

    // Empty exterior ring
    test_centroid_exception<bg::model::polygon<P> >(
        "POLYGON((), ())");
    test_centroid_exception<bg::model::polygon<P> >(
        "POLYGON((), (0 0, 1 0, 1 1, 0 1, 0 0))");
}

template <typename P>
void test_empty()
{
    // Empty interior ring
    test_centroid<bg::model::polygon<P> >(
        "POLYGON((0 0, 1 0, 1 1, 0 1, 0 0), ())",
        0.5, 0.5);
}

void test_large_integers()
{
    typedef bg::model::point<int, 2, bg::cs::cartesian> int_point_type;
    typedef bg::model::point<double, 2, bg::cs::cartesian> double_point_type;

    bg::model::polygon<int_point_type> int_poly;
    bg::model::polygon<double_point_type> double_poly;

    std::string const polygon_li = "POLYGON((1872000 528000,1872000 192000,1536119 192000,1536000 528000,1200000 528000,1200000 863880,1536000 863880,1872000 863880,1872000 528000))";
    bg::read_wkt(polygon_li, int_poly);
    bg::read_wkt(polygon_li, double_poly);

    int_point_type int_centroid;
    double_point_type double_centroid;

    bg::centroid(int_poly, int_centroid);
    bg::centroid(double_poly, double_centroid);

    int_point_type double_centroid_as_int;
    bg::assign_zero(double_centroid_as_int);
    bg::assign(int_centroid, double_centroid_as_int);

    BOOST_CHECK_EQUAL(bg::get<0>(int_centroid), bg::get<0>(double_centroid_as_int));
    BOOST_CHECK_EQUAL(bg::get<1>(int_centroid), bg::get<1>(double_centroid_as_int));
}

//#include <to_svg.hpp>

void test_large_doubles()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point;
    point pt_far, pt_near;
    bg::model::polygon<point> poly_far, poly_near;

    // related to ticket #10643
    bg::read_wkt("POLYGON((1074699.93 703064.65, 1074703.90 703064.58, 1074704.53 703061.40, 1074702.10 703054.62, 1074699.93 703064.65))", poly_far);
    bg::read_wkt("POLYGON((699.93 64.65, 703.90 64.58, 704.53 61.40, 702.10 54.62, 699.93 64.65))", poly_near);

    bg::centroid(poly_far, pt_far);
    bg::centroid(poly_near, pt_near);

    BOOST_CHECK(bg::within(pt_far, poly_far));
    BOOST_CHECK(bg::within(pt_near, poly_near));

    point pt_near_moved;
    bg::set<0>(pt_near_moved, bg::get<0>(pt_near) + 1074000.0);
    bg::set<1>(pt_near_moved, bg::get<1>(pt_near) + 703000.0);

    //geom_to_svg(poly_far, pt_far, "far.svg");
    //geom_to_svg(poly_near, pt_near, "near.svg");

    double d = bg::distance(pt_far, pt_near_moved);
    BOOST_CHECK(d < 0.1);
}

int test_main(int, char* [])
{
    test_2d<bg::model::d2::point_xy<double> >();
    test_2d<boost::tuple<float, float> >();
    test_2d<bg::model::d2::point_xy<float> >();

    test_3d<boost::tuple<double, double, double> >();

    test_5d<boost::tuple<double, double, double, double, double> >();

#if defined(HAVE_TTMATH)
    test_2d<bg::model::d2::point_xy<ttmath_big> >();
    test_3d<boost::tuple<ttmath_big, ttmath_big, ttmath_big> >();
#endif

#ifndef NDEBUG
    // The test currently fails in release mode. TODO: fix this
    test_large_integers();
#endif

    test_large_doubles();

    test_exceptions<bg::model::d2::point_xy<double> >();
    test_empty<bg::model::d2::point_xy<double> >();

    return 0;
}
