// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2014 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2014 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2014 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014-2021.
// Modifications copyright (c) 2014-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <sstream>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/comparable_distance.hpp>
#include <boost/geometry/algorithms/make.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/strategies/strategies.hpp>

template <typename P>
void test_distance_result()
{
    typedef typename bg::default_distance_result<P, P>::type distance_type;

    P p1 = bg::make<P>(0, 0);
    P p2 = bg::make<P>(3, 0);
    P p3 = bg::make<P>(0, 4);

    distance_type dr12 = bg::comparable_distance(p1, p2);
    distance_type dr13 = bg::comparable_distance(p1, p3);
    distance_type dr23 = bg::comparable_distance(p2, p3);

    BOOST_CHECK_CLOSE(dr12, 9.000, 0.001);
    BOOST_CHECK_CLOSE(dr13, 16.000, 0.001);
    BOOST_CHECK_CLOSE(dr23, 25.000, 0.001);

}

template <typename P>
void test_distance_point()
{
    P p1;
    bg::set<0>(p1, 1);
    bg::set<1>(p1, 1);

    P p2;
    bg::set<0>(p2, 2);
    bg::set<1>(p2, 2);

    typename bg::coordinate_type<P>::type d = bg::comparable_distance(p1, p2);
    BOOST_CHECK_CLOSE(d, 2.0, 0.001);
}

template <typename P>
void test_distance_segment()
{
    typedef typename bg::coordinate_type<P>::type coordinate_type;

    P s1 = bg::make<P>(2, 2);
    P s2 = bg::make<P>(3, 3);

    // Check points left, right, projected-left, projected-right, on segment
    P p1 = bg::make<P>(0, 0);
    P p2 = bg::make<P>(4, 4);
    P p3 = bg::make<P>(2.4, 2.6);
    P p4 = bg::make<P>(2.6, 2.4);
    P p5 = bg::make<P>(2.5, 2.5);

    bg::model::referring_segment<P const> const seg(s1, s2);

    coordinate_type d1 = bg::comparable_distance(p1, seg); BOOST_CHECK_CLOSE(d1, 8.0, 0.001);
    coordinate_type d2 = bg::comparable_distance(p2, seg); BOOST_CHECK_CLOSE(d2, 2.0, 0.001);
    coordinate_type d3 = bg::comparable_distance(p3, seg); BOOST_CHECK_CLOSE(d3, 0.02, 0.001);
    coordinate_type d4 = bg::comparable_distance(p4, seg); BOOST_CHECK_CLOSE(d4, 0.02, 0.001);
    coordinate_type d5 = bg::comparable_distance(p5, seg); BOOST_CHECK_CLOSE(d5, 0.0, 0.001);

    // Reverse case
    coordinate_type dr1 = bg::comparable_distance(seg, p1); BOOST_CHECK_CLOSE(dr1, d1, 0.001);
    coordinate_type dr2 = bg::comparable_distance(seg, p2); BOOST_CHECK_CLOSE(dr2, d2, 0.001);
}

template <typename P>
void test_distance_linestring()
{
    bg::model::linestring<P> points;
    points.push_back(bg::make<P>(1, 1));
    points.push_back(bg::make<P>(3, 3));

    P p = bg::make<P>(2, 1);

    typename bg::coordinate_type<P>::type d = bg::comparable_distance(p, points);
    BOOST_CHECK_CLOSE(d, 0.5, 0.001);

    p = bg::make<P>(5, 5);
    d = bg::comparable_distance(p, points);
    BOOST_CHECK_CLOSE(d, 8.0, 0.001);


    bg::model::linestring<P> line;
    line.push_back(bg::make<P>(1,1));
    line.push_back(bg::make<P>(2,2));
    line.push_back(bg::make<P>(3,3));

    p = bg::make<P>(5, 5);

    d = bg::comparable_distance(p, line);
    BOOST_CHECK_CLOSE(d, 8.0, 0.001);

    // Reverse case
    d = bg::comparable_distance(line, p);
    BOOST_CHECK_CLOSE(d, 8.0, 0.001);
}

template <typename P>
void test_all()
{
    test_distance_result<P>();
    test_distance_point<P>();
    test_distance_segment<P>();
    test_distance_linestring<P>();
}

template <typename T>
void test_double_result_from_integer()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;

    point_type point;

    // Check linestring
    bg::model::linestring<point_type> linestring;
    bg::read_wkt("POINT(2 2)", point);
    bg::read_wkt("LINESTRING(4 1,1 4)", linestring);

    double normal_distance = bg::distance(point, linestring);
    double comparable_distance = bg::comparable_distance(point, linestring);

    BOOST_CHECK_CLOSE(normal_distance, std::sqrt(0.5), 0.001);
    BOOST_CHECK_CLOSE(comparable_distance, 0.5, 0.001);

    // Check polygon
    bg::model::polygon<point_type> polygon;
    bg::read_wkt("POLYGON((0 0,1 9,8 1,0 0),(1 1,4 1,1 4,1 1))", polygon);

    normal_distance = bg::distance(point, polygon);
    comparable_distance = bg::comparable_distance(point, polygon);

    BOOST_CHECK_CLOSE(normal_distance, std::sqrt(0.5), 0.001);
    BOOST_CHECK_CLOSE(comparable_distance, 0.5, 0.001);
}

template <typename T>
struct test_variant_different_default_strategy
{
    static inline void apply()
    {
        typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
        typedef bg::model::segment<point_type> segment_type;
        typedef bg::model::box<point_type> box_type;
        typedef boost::variant<point_type, segment_type, box_type> variant_type;

        point_type point;
        bg::read_wkt("POINT(1 3)", point);

        segment_type seg;
        bg::read_wkt("LINESTRING(1 1,4 4)", seg);

        box_type box;
        bg::read_wkt("BOX(-1 -1,0 0)", box);

        variant_type v1, v2;
    
        using variant_cdistance_t = typename bg::comparable_distance_result
            <
                variant_type, variant_type, bg::default_strategy
            >::type;
        using cdistance_t =  typename bg::comparable_distance_result
            <
                point_type, point_type, bg::default_strategy
            >::type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<variant_cdistance_t, cdistance_t>::value),
            "Unexpected result type",
            variant_cdistance_t, cdistance_t);

        // Default strategy
        v1 = point;
        v2 = point;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2),
                          bg::comparable_distance(point, point),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, point),
                          bg::comparable_distance(point, point),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(point, v2),
                          bg::comparable_distance(point, point),
                          0.0001);
        v1 = point;
        v2 = seg;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2),
                          bg::comparable_distance(point, seg),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, seg),
                          bg::comparable_distance(point, seg),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(point, v2),
                          bg::comparable_distance(point, seg), 0.0001);
        v1 = point;
        v2 = box;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2),
                          bg::comparable_distance(point, box),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, box),
                          bg::comparable_distance(point, box),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(point, v2),
                          bg::comparable_distance(point, box), 0.0001);
    }
};

template <typename T, typename ExpectedResultType = double>
struct test_variant_same_default_strategy
{
    static inline void apply()
    {
        typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
        typedef bg::model::segment<point_type> segment_type;
        typedef bg::model::linestring<point_type> linestring_type;
        typedef boost::variant
            <
                point_type, segment_type, linestring_type
            > variant_type;

        point_type point;
        bg::read_wkt("POINT(1 3)", point);

        segment_type seg;
        bg::read_wkt("LINESTRING(1 1,4 4)", seg);

        linestring_type linestring;
        bg::read_wkt("LINESTRING(-1 -1,-1 0,0 0,0 -1,-1 -1)", linestring);

        variant_type v1, v2;
    
        using variant_cdistance_t = typename bg::comparable_distance_result
            <
                variant_type, variant_type, bg::default_strategy
            >::type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<variant_cdistance_t, ExpectedResultType>::value),
            "Unexpected result type",
            variant_cdistance_t, ExpectedResultType);

        using cdistance_t = typename bg::comparable_distance_result
            <
                point_type, point_type, bg::default_strategy
            >::type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<cdistance_t, ExpectedResultType>::value),
            "Unexpected result type",
            cdistance_t, ExpectedResultType);

        // Default strategy
        v1 = point;
        v2 = point;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2),
                          bg::comparable_distance(point, point),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, point),
                          bg::comparable_distance(point, point),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(point, v2),
                          bg::comparable_distance(point, point),
                          0.0001);
        v1 = point;
        v2 = seg;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2),
                          bg::comparable_distance(point, seg),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, seg),
                          bg::comparable_distance(point, seg),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(point, v2),
                          bg::comparable_distance(point, seg),
                          0.0001);
        v1 = point;
        v2 = linestring;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2),
                          bg::comparable_distance(point, linestring),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, linestring),
                          bg::comparable_distance(point, linestring),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(point, v2),
                          bg::comparable_distance(point, linestring),
                          0.0001);
    }
};

template <typename T, typename ExpectedResultType = T>
struct test_variant_with_strategy
{
    static inline void apply()
    {
        typedef bg::strategy::distance::projected_point<T> strategy_type;

        typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
        typedef bg::model::segment<point_type> segment_type;
        typedef bg::model::linestring<point_type> linestring_type;
        typedef bg::model::multi_linestring
            <
                linestring_type
            > multi_linestring_type;
        typedef boost::variant
            <
                segment_type, linestring_type, multi_linestring_type
            > variant_type;

        segment_type seg;
        bg::read_wkt("LINESTRING(1 1,4 4)", seg);

        linestring_type ls;
        bg::read_wkt("LINESTRING(-1 -1,-1 0,0 0,0 -1,-1 -1)", ls);

        multi_linestring_type mls;
        bg::read_wkt("MULTILINESTRING((10 0,20 0),(30 0,40 0))", mls);

        variant_type v1, v2;

        strategy_type strategy;

        using variant_cdistance_t = typename bg::comparable_distance_result
            <
                variant_type, variant_type, strategy_type
            >::type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<variant_cdistance_t, ExpectedResultType>::value),
            "Unexpected result type",
            variant_cdistance_t, ExpectedResultType);

        using cdistance_t = typename bg::comparable_distance_result
            <
                segment_type, linestring_type, strategy_type
            >::type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<cdistance_t, ExpectedResultType>::value),
            "Unexpected result type",
            cdistance_t, ExpectedResultType);

        // Passed strategy
        v1 = seg;
        v2 = seg;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2, strategy),
                          bg::comparable_distance(seg, seg, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, seg, strategy),
                          bg::comparable_distance(seg, seg, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(seg, v2, strategy),
                          bg::comparable_distance(seg, seg, strategy),
                          0.0001);
        v1 = seg;
        v2 = ls;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2, strategy),
                          bg::comparable_distance(seg, ls, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, ls, strategy),
                          bg::comparable_distance(seg, ls, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(seg, v2, strategy),
                          bg::comparable_distance(seg, ls, strategy),
                          0.0001);
        v1 = seg;
        v2 = mls;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2, strategy),
                          bg::comparable_distance(seg, mls, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, mls, strategy),
                          bg::comparable_distance(seg, mls, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(seg, v2, strategy),
                          bg::comparable_distance(seg, mls, strategy),
                          0.0001);
        v1 = ls;
        v2 = mls;
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, v2, strategy),
                          bg::comparable_distance(ls, mls, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(v1, mls, strategy),
                          bg::comparable_distance(ls, mls, strategy),
                          0.0001);
        BOOST_CHECK_CLOSE(bg::comparable_distance(ls, v2, strategy),
                          bg::comparable_distance(ls, mls, strategy),
                          0.0001);
    }
};

template <typename T, bool IsIntergral = std::is_integral<T>::value>
struct check_result
{
    template <typename ExpectedResult>
    static inline void apply(T const& value,
                             ExpectedResult const& expected_value)
    {
        BOOST_CHECK_EQUAL(value, expected_value);
    }
};

template <typename T>
struct check_result<T, false>
{
    template <typename ExpectedResult>
    static inline void apply(T const& value,
                             ExpectedResult const& expected_value)
    {
        BOOST_CHECK_CLOSE(value, expected_value, 0.0001);
    }
};

template <typename T>
struct test_variant_boxes
{
    static inline void apply()
    {
        typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
        typedef bg::model::box<point_type> box_type;
        typedef boost::variant<box_type> variant_type;

        box_type box1, box2;
        bg::read_wkt("BOX(-1 -1,0 0)", box1);
        bg::read_wkt("BOX(1 1,2 2)", box2);

        variant_type v1 = box1, v2 = box2;

        typedef typename std::conditional
            <
                std::is_floating_point<T>::value,
                double,
                typename bg::util::detail::default_integral::type
            >::type expected_result_type;

        using variant_cdistance_t = typename bg::comparable_distance_result
            <
                variant_type, variant_type, bg::default_strategy
            >::type;
        BOOST_GEOMETRY_STATIC_ASSERT(
            (std::is_same<variant_cdistance_t, expected_result_type>::value),
            "Unexpected result type",
            variant_cdistance_t, expected_result_type);

        // Default strategy
        check_result<T>::apply(bg::comparable_distance(v1, v2),
                               bg::comparable_distance(box1, box2));
        check_result<T>::apply(bg::comparable_distance(v1, box2),
                               bg::comparable_distance(box1, box2));
        check_result<T>::apply(bg::comparable_distance(box1, v2),
                               bg::comparable_distance(box1, box2));
    }
};


int test_main(int, char* [])
{
    test_double_result_from_integer<int>();
    test_double_result_from_integer<long long>();

    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

    // test variant support
    test_variant_different_default_strategy<double>::apply();

    test_variant_same_default_strategy<double>::apply();
    test_variant_same_default_strategy<int>::apply();
    test_variant_same_default_strategy<long>::apply();

    test_variant_with_strategy<double>::apply();
    test_variant_with_strategy<float>::apply();
    test_variant_with_strategy<long double>::apply();
    test_variant_with_strategy<int, double>::apply();

    test_variant_boxes<double>::apply();
    test_variant_boxes<int>::apply();
    test_variant_boxes<long>::apply();

    return 0;
}
