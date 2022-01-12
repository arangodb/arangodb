// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// Copyright (c) 2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//#include <boost/geometry/geometry.hpp>

#include <string>
#include <sstream>

#include "test_distance.hpp"

#include <boost/array.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>

#include <test_common/test_point.hpp>
#include <test_geometries/custom_segment.hpp>
#include <test_geometries/wrapped_boost_array.hpp>

#include <boost/variant/variant.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)

// Register boost array as a linestring
namespace boost { namespace geometry { namespace traits
{
template <typename Point, std::size_t PointCount>
struct tag< boost::array<Point, PointCount> >
{
    typedef linestring_tag type;
};

}}}

template <typename P>
void test_distance_point()
{
    namespace services = bg::strategy::distance::services;
    typedef typename bg::default_distance_result<P>::type return_type;

    // Basic, trivial test

    P p1;
    bg::set<0>(p1, 1);
    bg::set<1>(p1, 1);

    P p2;
    bg::set<0>(p2, 2);
    bg::set<1>(p2, 2);

    return_type d = bg::distance(p1, p2);
    BOOST_CHECK_CLOSE(d, return_type(1.4142135), 0.001);

    // Test specifying strategy manually
    typename services::default_strategy
        <
            bg::point_tag, bg::point_tag, P
        >::type strategy;

    d = bg::distance(p1, p2, strategy);
    BOOST_CHECK_CLOSE(d, return_type(1.4142135), 0.001);

    {
        // Test custom strategy
        BOOST_CONCEPT_ASSERT( (bg::concepts::PointDistanceStrategy<taxicab_distance, P, P>) );

        typedef typename services::return_type<taxicab_distance, P, P>::type cab_return_type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<cab_return_type, typename bg::coordinate_type<P>::type>::value),
            "Unexpected result type",
            cab_return_type, typename bg::coordinate_type<P>::type);

        taxicab_distance tcd;
        cab_return_type d = bg::distance(p1, p2, tcd);

        BOOST_CHECK( bg::math::abs(d - cab_return_type(2)) <= cab_return_type(0.01) );
    }

    {
        // test comparability

        typedef typename services::default_strategy
            <
                bg::point_tag, bg::point_tag, P
            >::type strategy_type;
        typedef typename services::comparable_type<strategy_type>::type comparable_strategy_type;

        strategy_type strategy;
        comparable_strategy_type comparable_strategy = services::get_comparable<strategy_type>::apply(strategy);
        return_type comparable = services::result_from_distance<comparable_strategy_type, P, P>::apply(comparable_strategy, 3);

        BOOST_CHECK_CLOSE(comparable, return_type(9), 0.001);
    }
}

template <typename P>
void test_distance_segment()
{
    typedef typename bg::default_distance_result<P>::type return_type;

    P s1; bg::set<0>(s1, 1); bg::set<1>(s1, 1);
    P s2; bg::set<0>(s2, 4); bg::set<1>(s2, 4);

    // Check points left, right, projected-left, projected-right, on segment
    P p1; bg::set<0>(p1, 0); bg::set<1>(p1, 1);
    P p2; bg::set<0>(p2, 1); bg::set<1>(p2, 0);
    P p3; bg::set<0>(p3, 3); bg::set<1>(p3, 1);
    P p4; bg::set<0>(p4, 1); bg::set<1>(p4, 3);
    P p5; bg::set<0>(p5, 3); bg::set<1>(p5, 3);

    bg::model::referring_segment<P const> const seg(s1, s2);

    return_type d1 = bg::distance(p1, seg);
    return_type d2 = bg::distance(p2, seg);
    return_type d3 = bg::distance(p3, seg);
    return_type d4 = bg::distance(p4, seg);
    return_type d5 = bg::distance(p5, seg);

    BOOST_CHECK_CLOSE(d1, return_type(1), 0.001);
    BOOST_CHECK_CLOSE(d2, return_type(1), 0.001);
    BOOST_CHECK_CLOSE(d3, return_type(1.4142135), 0.001);
    BOOST_CHECK_CLOSE(d4, return_type(1.4142135), 0.001);
    BOOST_CHECK_CLOSE(d5, return_type(0), 0.001);

    // Reverse case: segment/point instead of point/segment
    return_type dr1 = bg::distance(seg, p1);
    return_type dr2 = bg::distance(seg, p2);

    BOOST_CHECK_CLOSE(dr1, d1, 0.001);
    BOOST_CHECK_CLOSE(dr2, d2, 0.001);

    // Test specifying strategy manually:
    // 1) point-point-distance
    typename bg::strategy::distance::services::default_strategy
        <
            bg::point_tag, bg::point_tag, P
        >::type pp_strategy;
    d1 = bg::distance(p1, seg, pp_strategy);
    BOOST_CHECK_CLOSE(d1, return_type(1), 0.001);

    // 2) point-segment-distance
    typename bg::strategy::distance::services::default_strategy
        <
            bg::point_tag, bg::segment_tag, P
        >::type ps_strategy;
    d1 = bg::distance(p1, seg, ps_strategy);
    BOOST_CHECK_CLOSE(d1, return_type(1), 0.001);

    // 3) custom point strategy
    taxicab_distance tcd;
    d1 = bg::distance(p1, seg, tcd);
    BOOST_CHECK_CLOSE(d1, return_type(1), 0.001);
}

template <typename Point, typename Geometry, typename T>
void test_distance_linear(std::string const& wkt_point, std::string const& wkt_geometry, T const& expected)
{
    Point p;
    bg::read_wkt(wkt_point, p);

    Geometry g;
    bg::read_wkt(wkt_geometry, g);

    typedef typename bg::default_distance_result<Point>::type return_type;
    return_type d = bg::distance(p, g);

    // For point-to-linestring (or point-to-polygon), both a point-strategy and a point-segment-strategy can be specified.
    // Test this.
    return_type ds1 = bg::distance(p, g, bg::strategy::distance::pythagoras<>());
    return_type ds2 = bg::distance(p, g, bg::strategy::distance::projected_point<>());

    BOOST_CHECK_CLOSE(d, return_type(expected), 0.001);
    BOOST_CHECK_CLOSE(ds1, return_type(expected), 0.001);
    BOOST_CHECK_CLOSE(ds2, return_type(expected), 0.001);
}

template <typename P>
void test_distance_array_as_linestring()
{
    typedef typename bg::default_distance_result<P>::type return_type;

    // Normal array does not have
    boost::array<P, 2> points;
    bg::set<0>(points[0], 1);
    bg::set<1>(points[0], 1);
    bg::set<0>(points[1], 3);
    bg::set<1>(points[1], 3);

    P p;
    bg::set<0>(p, 2);
    bg::set<1>(p, 1);

    return_type d = bg::distance(p, points);
    BOOST_CHECK_CLOSE(d, return_type(0.70710678), 0.001);

    bg::set<0>(p, 5); bg::set<1>(p, 5);
    d = bg::distance(p, points);
    BOOST_CHECK_CLOSE(d, return_type(2.828427), 0.001);
}


// code moved from the distance unit test in multi/algorithms -- start
template <typename Geometry1, typename Geometry2>
void test_distance(std::string const& wkt1, std::string const& wkt2, double expected)
{
    Geometry1 g1;
    Geometry2 g2;
    bg::read_wkt(wkt1, g1);
    bg::read_wkt(wkt2, g2);
    typename bg::default_distance_result<Geometry1, Geometry2>::type d = bg::distance(g1, g2);

    BOOST_CHECK_CLOSE(d, expected, 0.0001);
}

template <typename Geometry1, typename Geometry2, typename Strategy>
void test_distance(Strategy const& strategy, std::string const& wkt1,
                   std::string const& wkt2, double expected)
{
    Geometry1 g1;
    Geometry2 g2;
    bg::read_wkt(wkt1, g1);
    bg::read_wkt(wkt2, g2);
    typename bg::default_distance_result<Geometry1, Geometry2>::type d = bg::distance(g1, g2, strategy);

    BOOST_CHECK_CLOSE(d, expected, 0.0001);
}


template <typename P>
void test_2d()
{
    typedef bg::model::multi_point<P> mp;
    typedef bg::model::multi_linestring<bg::model::linestring<P> > ml;
    test_distance<P, P>("POINT(0 0)", "POINT(1 1)", sqrt(2.0));
    test_distance<P, mp>("POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<mp, P>("MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp, mp>("MULTIPOINT((1 1),(1 0),(0 2))", "MULTIPOINT((2 2),(2 3))", sqrt(2.0));
    test_distance<P, ml>("POINT(0 0)", "MULTILINESTRING((1 1,2 2),(1 0,2 0),(0 2,0 3))", 1.0);
    test_distance<ml, P>("MULTILINESTRING((1 1,2 2),(1 0,2 0),(0 2,0 3))", "POINT(0 0)", 1.0);
    test_distance<ml, mp>("MULTILINESTRING((1 1,2 2),(1 0,2 0),(0 2,0 3))", "MULTIPOINT((0 0),(1 1))", 0.0);

    // Test with a strategy
    bg::strategy::distance::pythagoras<> pyth;
    test_distance<P, P>(pyth, "POINT(0 0)", "POINT(1 1)", sqrt(2.0));
    test_distance<P, mp>(pyth, "POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<mp, P>(pyth, "MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
}


template <typename P>
void test_3d()
{
    typedef bg::model::multi_point<P> mp;
    test_distance<P, P>("POINT(0 0 0)", "POINT(1 1 1)", sqrt(3.0));
    test_distance<P, mp>("POINT(0 0 0)", "MULTIPOINT((1 1 1),(1 0 0),(0 1 2))", 1.0);
    test_distance<mp, mp>("MULTIPOINT((1 1 1),(1 0 0),(0 0 2))", "MULTIPOINT((2 2 2),(2 3 4))", sqrt(3.0));
}


template <typename P1, typename P2>
void test_mixed()
{
    typedef bg::model::multi_point<P1> mp1;
    typedef bg::model::multi_point<P2> mp2;

    test_distance<P1, P2>("POINT(0 0)", "POINT(1 1)", sqrt(2.0));

    test_distance<P1, mp1>("POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<P1, mp2>("POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<P2, mp1>("POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<P2, mp2>("POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);

    // Test automatic reversal
    test_distance<mp1, P1>("MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp1, P2>("MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp2, P1>("MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp2, P2>("MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);

    // Test multi-multi using different point types for each
    test_distance<mp1, mp2>("MULTIPOINT((1 1),(1 0),(0 2))", "MULTIPOINT((2 2),(2 3))", sqrt(2.0));

    // Test with a strategy
    using namespace bg::strategy::distance;

    test_distance<P1, P2>(pythagoras<>(), "POINT(0 0)", "POINT(1 1)", sqrt(2.0));

    test_distance<P1, mp1>(pythagoras<>(), "POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<P1, mp2>(pythagoras<>(), "POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<P2, mp1>(pythagoras<>(), "POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);
    test_distance<P2, mp2>(pythagoras<>(), "POINT(0 0)", "MULTIPOINT((1 1),(1 0),(0 2))", 1.0);

    // Most interesting: reversal AND a strategy (note that the stategy must be reversed automatically
    test_distance<mp1, P1>(pythagoras<>(), "MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp1, P2>(pythagoras<>(), "MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp2, P1>(pythagoras<>(), "MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
    test_distance<mp2, P2>(pythagoras<>(), "MULTIPOINT((1 1),(1 0),(0 2))", "POINT(0 0)", 1.0);
}
// code moved from the distance unit test in multi/algorithms -- end




template <typename P>
void test_all()
{
    test_distance_point<P>();
    test_distance_segment<P>();
    test_distance_array_as_linestring<P>();

    test_geometry<P, bg::model::segment<P> >("POINT(1 3)", "LINESTRING(1 1,4 4)", sqrt(2.0));
    test_geometry<P, bg::model::segment<P> >("POINT(3 1)", "LINESTRING(1 1,4 4)", sqrt(2.0));

    test_geometry<P, P>("POINT(1 1)", "POINT(2 2)", sqrt(2.0));
    test_geometry<P, P>("POINT(0 0)", "POINT(0 3)", 3.0);
    test_geometry<P, P>("POINT(0 0)", "POINT(4 0)", 4.0);
    test_geometry<P, P>("POINT(0 3)", "POINT(4 0)", 5.0);
    test_geometry<P, bg::model::linestring<P> >("POINT(1 3)", "LINESTRING(1 1,4 4)", sqrt(2.0));
    test_geometry<P, bg::model::linestring<P> >("POINT(3 1)", "LINESTRING(1 1,4 4)", sqrt(2.0));
    test_geometry<P, bg::model::linestring<P> >("POINT(50 50)", "LINESTRING(50 40, 40 50)", sqrt(50.0));
    test_geometry<P, bg::model::linestring<P> >("POINT(50 50)", "LINESTRING(50 40, 40 50, 0 90)", sqrt(50.0));
    test_geometry<bg::model::linestring<P>, P>("LINESTRING(1 1,4 4)", "POINT(1 3)", sqrt(2.0));
    test_geometry<bg::model::linestring<P>, P>("LINESTRING(50 40, 40 50)", "POINT(50 50)", sqrt(50.0));
    test_geometry<bg::model::linestring<P>, P>("LINESTRING(50 40, 40 50, 0 90)", "POINT(50 50)", sqrt(50.0));

    // Rings
    test_geometry<P, bg::model::ring<P> >("POINT(1 3)", "POLYGON((1 1,4 4,5 0,1 1))", sqrt(2.0));
    test_geometry<P, bg::model::ring<P> >("POINT(3 1)", "POLYGON((1 1,4 4,5 0,1 1))", 0.0);
    // other way round
    test_geometry<bg::model::ring<P>, P>("POLYGON((1 1,4 4,5 0,1 1))", "POINT(3 1)", 0.0);
    // open ring
    test_geometry<P, bg::model::ring<P, true, false> >("POINT(1 3)", "POLYGON((4 4,5 0,1 1))", sqrt(2.0));

    // Polygons
    test_geometry<P, bg::model::polygon<P> >("POINT(1 3)", "POLYGON((1 1,4 4,5 0,1 1))", sqrt(2.0));
    test_geometry<P, bg::model::polygon<P> >("POINT(3 1)", "POLYGON((1 1,4 4,5 0,1 1))", 0.0);
    // other way round
    test_geometry<bg::model::polygon<P>, P>("POLYGON((1 1,4 4,5 0,1 1))", "POINT(3 1)", 0.0);
    // open polygon
    test_geometry<P, bg::model::polygon<P, true, false> >("POINT(1 3)", "POLYGON((4 4,5 0,1 1))", sqrt(2.0));

    // Polygons with holes
    std::string donut = "POLYGON ((0 0,1 9,8 1,0 0),(1 1,4 1,1 4,1 1))";
    test_geometry<P, bg::model::polygon<P> >("POINT(2 2)", donut, 0.5 * sqrt(2.0));
    test_geometry<P, bg::model::polygon<P> >("POINT(3 3)", donut, 0.0);
    // other way round
    test_geometry<bg::model::polygon<P>, P>(donut, "POINT(2 2)", 0.5 * sqrt(2.0));
    // open
    test_geometry<P, bg::model::polygon<P, true, false> >("POINT(2 2)", "POLYGON ((0 0,1 9,8 1),(1 1,4 1,1 4))", 0.5 * sqrt(2.0));

    // Should (currently) give compiler assertion
    // test_geometry<bg::model::polygon<P>, bg::model::polygon<P> >(donut, donut, 0.5 * sqrt(2.0));

    // DOES NOT COMPILE - cannot do read_wkt (because boost::array is not variably sized)
    // test_geometry<P, boost::array<P, 2> >("POINT(3 1)", "LINESTRING(1 1,4 4)", sqrt(2.0));

    test_geometry<P, test::wrapped_boost_array<P, 2> >("POINT(3 1)", "LINESTRING(1 1,4 4)", sqrt(2.0));

    test_distance_linear<P, bg::model::linestring<P> >("POINT(3 1)", "LINESTRING(1 1,4 4)", sqrt(2.0));
}

template <typename P>
void test_empty_input()
{
    P p;
    bg::model::linestring<P> line_empty;
    bg::model::polygon<P> poly_empty;
    bg::model::ring<P> ring_empty;
    bg::model::multi_point<P> mp_empty;
    bg::model::multi_linestring<bg::model::linestring<P> > ml_empty;

    test_empty_input(p, line_empty);
    test_empty_input(p, poly_empty);
    test_empty_input(p, ring_empty);

    test_empty_input(p, mp_empty);
    test_empty_input(p, ml_empty);
    test_empty_input(mp_empty, mp_empty);

    // Test behaviour if one of the inputs is empty
    bg::model::multi_point<P> mp;
    mp.push_back(p);
    test_empty_input(mp_empty, mp);
    test_empty_input(mp, mp_empty);
}

void test_large_integers()
{
    typedef bg::model::point<int, 2, bg::cs::cartesian> int_point_type;
    typedef bg::model::point<double, 2, bg::cs::cartesian> double_point_type;

    // point-point
    {
        std::string const a = "POINT(2544000 528000)";
        std::string const b = "POINT(2768040 528000)";
        int_point_type ia, ib;
        double_point_type da, db;
        bg::read_wkt(a, ia);
        bg::read_wkt(b, ib);
        bg::read_wkt(a, da);
        bg::read_wkt(b, db);

        auto const idist = bg::distance(ia, ib);
        auto const ddist = bg::distance(da, db);

        BOOST_CHECK_MESSAGE(std::abs(idist - ddist) < 0.1,
                "within<a double> different from within<an int>");
    }
    // Point-segment
    {
        std::string const a = "POINT(2600000 529000)";
        std::string const b = "LINESTRING(2544000 528000, 2768040 528000)";
        int_point_type ia;
        double_point_type da;
        bg::model::segment<int_point_type> ib;
        bg::model::segment<double_point_type> db;
        bg::read_wkt(a, ia);
        bg::read_wkt(b, ib);
        bg::read_wkt(a, da);
        bg::read_wkt(b, db);

        auto const idist = bg::distance(ia, ib);
        auto const ddist = bg::distance(da, db);

        BOOST_CHECK_MESSAGE(std::abs(idist - ddist) < 0.1,
                "within<a double> different from within<an int>");
    }
}

template <typename T>
void test_variant()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    typedef bg::model::segment<point_type> segment_type;
    typedef bg::model::box<point_type> box_type;
    typedef boost::variant<point_type, segment_type, box_type> variant_type;

    point_type point;
    std::string const point_li = "POINT(1 3)";
    bg::read_wkt(point_li, point);

    segment_type seg;
    std::string const seg_li = "LINESTRING(1 1,4 4)";
    bg::read_wkt(seg_li, seg);

    variant_type v1, v2;
    
    using distance_t = typename bg::distance_result
        <
            variant_type, variant_type, bg::default_strategy
        >::type;
    BOOST_GEOMETRY_STATIC_ASSERT(
        (std::is_same<distance_t, double>::value),
        "Unexpected result type",
        distance_t, double);

    // Default strategy
    v1 = point;
    v2 = point;
    BOOST_CHECK_CLOSE(bg::distance(v1, v2), bg::distance(point, point), 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(v1, point), bg::distance(point, point), 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(point, v2), bg::distance(point, point), 0.0001);
    v1 = point;
    v2 = seg;
    BOOST_CHECK_CLOSE(bg::distance(v1, v2), bg::distance(point, seg), 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(v1, seg), bg::distance(point, seg), 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(point, v2), bg::distance(point, seg), 0.0001);

    // User defined strategy
    v1 = point;
    v2 = point;
    bg::strategy::distance::haversine<double> s;
    //BOOST_CHECK_CLOSE(bg::distance(v1, v2, s), bg::distance(point, point, s), 0.0001);
    //BOOST_CHECK_CLOSE(bg::distance(v1, point, s), bg::distance(point, point, s), 0.0001);
    //BOOST_CHECK_CLOSE(bg::distance(point, v2, s), bg::distance(point, point, s), 0.0001);
}

template <typename T>
void test_geometry_collection()
{
    using point_type = bg::model::point<T, 2, bg::cs::cartesian>;
    using segment_type = bg::model::segment<point_type>;
    using box_type = bg::model::box<point_type>;
    using variant_type = boost::variant<point_type, segment_type, box_type>;
    using gc_type = bg::model::geometry_collection<variant_type>;

    point_type p1 {1, 3}, p2 {2, 3};
    segment_type s1 {{2, 2}, {4, 4}}, s2 {{3, 2}, {5, 4}};
    gc_type gc1 {p1, s1}, gc2 {p2, s2};

    BOOST_CHECK_CLOSE(bg::distance(p1, gc2), bg::distance(p1, p2), 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(gc1, s2), bg::distance(s1, s2), 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(gc1, gc2), bg::distance(s1, s2), 0.0001);
}

int test_main(int, char* [])
{
#ifdef TEST_ARRAY
    //test_all<int[2]>();
    //test_all<float[2]>();
    //test_all<double[2]>();
    //test_all<test::test_point>(); // located here because of 3D
#endif

    test_large_integers();

    test_all<bg::model::d2::point_xy<int> >();
    test_all<boost::tuple<float, float> >();
    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

    test_empty_input<bg::model::d2::point_xy<int> >();

    // below are the test cases moved here from the distance unit test
    // in test/multi/algorithms
    test_2d<boost::tuple<float, float> >();
    test_2d<bg::model::d2::point_xy<float> >();
    test_2d<bg::model::d2::point_xy<double> >();

    test_3d<boost::tuple<float, float, float> >();
    test_3d<bg::model::point<double, 3, bg::cs::cartesian> >();

    test_mixed<bg::model::d2::point_xy<float>, bg::model::d2::point_xy<double> >();

    test_empty_input<bg::model::d2::point_xy<int> >();

    test_variant<double>();
    test_variant<int>();

    test_geometry_collection<double>();

    return 0;
}
