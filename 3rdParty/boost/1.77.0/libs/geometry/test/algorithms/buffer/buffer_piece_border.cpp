// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2020 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// htt//www.boost.org/LICENSE_1_0.txt)

#include "geometry_test_common.hpp"

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/buffer/piece_border.hpp>
#include <boost/geometry/util/range.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/strategies/comparable_distance_result.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras.hpp>
#include <boost/geometry/strategies/cartesian/distance_projected_point.hpp>

// TEMP
#include <boost/geometry/strategies/cartesian.hpp>

#if defined(TEST_WITH_SVG)
#include <boost/geometry/io/svg/svg_mapper.hpp>
#include <boost/geometry/algorithms/detail/buffer/buffer_box.hpp>
#endif

static const std::string rectangle_offsetted = "POLYGON((0 0,0 1,0 2,0 3,1 3,2 3,3 3,3 2,3 1,3 0,2 0,1 0,0 0))";
static const std::string rectangle_original = "POLYGON((1 1,1 2,2 2,2 1,1 1))";

static const std::string diamond_offsetted = "POLYGON((3 0,2 1,1 2,0 3,1 4,2 5,3 6,4 5,5 4,6 3,5 2,4 1,3 0))";
static const std::string diamond_original = "POLYGON((3 2,2 3,3 4,4 3,3 2))";

template <typename Geometry>
inline std::string as_wkt(Geometry const& geometry)
{
    std::ostringstream out;
    out << bg::wkt(geometry);
    return out.str();
}

// Return the border. It needs to pass the ring because its address
// is preserved in the border. It also gets the original.
template <typename Border, typename Ring>
Border setup_piece_border(Ring& ring, Ring& original,
                          std::string const& offsetted_wkt,
                          std::string const& original_wkt,
                          char piece)
{
    // The test setup (here fore case "rectangle"; "diamond" has same layout).
    // Points on offsetted ring are numbered, points on original like this [0].
    // a, b, c: test pieces.
    //
    //           4       5
    //   3 +-----+-------+-----+ 6
    //     |     |       |     |
    //     |     |   a   |  b  |
    //     |     |       |     |
    //   2 +----[1]-----[2]----+ 7
    //     |     |       |     |
    //     |     |  ori  |  c  |
    //     |     |       |     |
    //   1 +----[0,4]---[3]----+ 8
    //     |     |       |     |
    //     |     |       |     |
    //     |     |       |     |
    //   0 +-----+-------+-----+ 9
    //    12    11       10


    bg::read_wkt(original_wkt, original);
    bg::read_wkt(offsetted_wkt, ring);

    Border border;

    switch(piece)
    {
        case 'a' :
            border.set_offsetted(ring, 4, 6);
            border.add_original_point(original[2]);
            border.add_original_point(original[1]);
            break;
        case 'b' :
            border.set_offsetted(ring, 5, 8);
            border.add_original_point(original[2]);
            break;
        case 'c' :
            border.set_offsetted(ring, 7, 9);
            border.add_original_point(original[3]);
            border.add_original_point(original[2]);
            break;
        default :
            return border;
    }

    typedef typename bg::point_type<Ring>::type point_type;

    bg::strategies::relate::cartesian<> strategy;

    border.get_properties_of_border(false, point_type(), strategy);

    border.get_properties_of_offsetted_ring_part(strategy);

    return border;
}

template <typename Point>
void test_rectangle_properties()
{
    typedef bg::model::ring<Point> ring_type;
    typedef bg::detail::buffer::piece_border<ring_type, Point> border_type;

    border_type empty;
    BOOST_CHECK_MESSAGE(empty.ring_or_original_empty(), "piece should be empty");

    ring_type offsetted, original;
    border_type border = setup_piece_border<border_type>(offsetted, original,
            rectangle_offsetted, rectangle_original, 'a');

    // Check get_full_ring functionality (used for testing, writing SVG)
    ring_type const outline = border.get_full_ring();
    std::string const outline_wkt = as_wkt(outline);

    std::string const expected = "POLYGON((1 3,2 3,2 2,1 2,1 3))";
    BOOST_CHECK_MESSAGE(outline_wkt == expected,
                        "expected: " << expected
                        << " detected: " << outline_wkt);

    BOOST_CHECK_MESSAGE(! border.ring_or_original_empty(),
                        "piece should not be empty");

    // Check border-properties functionality (envelope, radius)
    auto const area = bg::area(border.m_envelope);
    BOOST_CHECK_MESSAGE(area > 1.0 && area < 1.01,
                        "detected: " << area);

    // Check offsetted-ring-properties functionality (convexity, monotonicity)
    BOOST_CHECK_MESSAGE(border.m_is_convex == true,
                        "piece should be convex");
    BOOST_CHECK_MESSAGE(border.m_is_monotonic_increasing == true,
                        "piece should be monotonically increasing");
    BOOST_CHECK_MESSAGE(border.m_is_monotonic_decreasing == false,
                        "piece NOT should be monotonically decreasing");
}

template <typename Point, typename Border, typename Mapper>
void test_point(std::string const& wkt, bool expected, Border const& border,
                Mapper& mapper, std::string const& color)
{
    typename Border::state_type state;
    Point point;
    bg::read_wkt(wkt, point);
    border.point_on_piece(point, false, false, state);
    bool const on_piece = state.code() == 1;
    BOOST_CHECK_MESSAGE(on_piece == expected,
                        " expected: " << expected
                        << " detected: " << on_piece
                        << " wkt: " << wkt);

#ifdef TEST_WITH_SVG
    std::string style = "fill:" + color + ";stroke:rgb(0,0,0);stroke-width:1";
    mapper.map(point, style);

    // Mark on-piece as T or F
    std::ostringstream out;
    out << (on_piece ? "T" : "F");
    mapper.text(point, out.str(), "fill:rgb(0,0,0);font-family='Arial';font-size:9px;", 10, -10);
#else
    boost::ignore_unused(mapper, color);
#endif
}

#ifdef TEST_WITH_SVG
template <typename Mapper, typename Ring, typename Border>
void start_svg(Mapper& mapper, Ring const& original, Ring const& offsetted,
               Border const& border)
{
    typedef typename bg::point_type<Ring>::type point_type;
    typedef bg::model::box<point_type> box_type;
    box_type box = border.m_envelope;
    bg::detail::buffer::buffer_box(box, 1.0, box);
    mapper.add(box);
    mapper.add(offsetted);
    mapper.map(offsetted,
               "fill-opacity:0.5;fill:rgb(255,255,128);stroke:none");
    mapper.map(original,
               "fill-opacity:0.5;fill:rgb(51,51,153);stroke:none");
    mapper.map(border.get_full_ring(),
               "fill-opacity:0.5;fill:rgb(153,204,0);"
               "stroke:rgb(153,204,0);stroke-width:1");
}
#endif

template <typename Point>
void test_rectangle_point_on_piece_a()
{
    typedef bg::model::ring<Point> ring_type;
    typedef bg::detail::buffer::piece_border<ring_type, Point> border_type;

    ring_type offsetted, original;
    border_type border = setup_piece_border<border_type>(offsetted, original,
            rectangle_offsetted, rectangle_original, 'a');

#ifdef TEST_WITH_SVG
    std::ofstream svg("/tmp/border_rectangle_a.svg");
    bg::svg_mapper<Point> mapper(svg, 500, 500);
    start_svg(mapper, original, offsetted, border);
#else
    int mapper = 0;
#endif

    // Points inside
    test_point<Point>("POINT(1.5 2.5)", true, border, mapper, "red");

    // Points outside
    test_point<Point>("POINT(0.5 2.5)", false, border, mapper, "green");
    test_point<Point>("POINT(2.5 2.5)", false, border, mapper, "green");
    test_point<Point>("POINT(1.5 1.5)", false, border, mapper, "green");
    test_point<Point>("POINT(1.5 3.5)", false, border, mapper, "green");

    // Points on the original (should be INSIDE)
    test_point<Point>("POINT(1.0 2.0)", true, border, mapper, "orange");
    test_point<Point>("POINT(1.5 2.0)", true, border, mapper, "orange");
    test_point<Point>("POINT(2.0 2.0)", true, border, mapper, "orange");

    // Points on the offsetted ring (should be OUTSIDE)
    test_point<Point>("POINT(1.0 3.0)", false, border, mapper, "blue");
    test_point<Point>("POINT(1.5 3.0)", false, border, mapper, "blue");
    test_point<Point>("POINT(2.0 3.0)", false, border, mapper, "blue");

    // Points on between original and offsetted ring (should be INSIDE)
    test_point<Point>("POINT(1.0 2.5)", true, border, mapper, "cyan");
    test_point<Point>("POINT(2.0 2.5)", true, border, mapper, "cyan");
}

template <typename Point>
void test_rectangle_point_on_piece_c()
{
    typedef bg::model::ring<Point> ring_type;
    typedef bg::detail::buffer::piece_border<ring_type, Point> border_type;

    ring_type offsetted, original;
    border_type border = setup_piece_border<border_type>(offsetted, original,
            rectangle_offsetted, rectangle_original, 'c');

#ifdef TEST_WITH_SVG
    std::ofstream svg("/tmp/border_rectangle_c.svg");
    bg::svg_mapper<Point> mapper(svg, 500, 500);
    start_svg(mapper, original, offsetted, border);
#else
    int mapper = 0;
#endif

    // Piece labeled as 'c' : from (2,1) to (3,2)

    // Points inside
    test_point<Point>("POINT(2.5 1.5)", true, border, mapper, "red");

    // Points outside
    test_point<Point>("POINT(1.5 1.5)", false, border, mapper, "green");
    test_point<Point>("POINT(3.5 1.5)", false, border, mapper, "green");
    test_point<Point>("POINT(2.5 0.5)", false, border, mapper, "green");
    test_point<Point>("POINT(2.5 2.5)", false, border, mapper, "green");

    // Points on the original (should be INSIDE)
    test_point<Point>("POINT(2.0 1.0)", true, border, mapper, "orange");
    test_point<Point>("POINT(2.0 1.5)", true, border, mapper, "orange");
    test_point<Point>("POINT(2.0 2.0)", true, border, mapper, "orange");

    // Points on the offsetted ring (should be OUTSIDE)
    test_point<Point>("POINT(3.0 1.0)", false, border, mapper, "blue");
    test_point<Point>("POINT(3.0 1.5)", false, border, mapper, "blue");
    test_point<Point>("POINT(3.0 2.0)", false, border, mapper, "blue");

    // Points on between original and offsetted ring (should be INSIDE)
    test_point<Point>("POINT(2.5 2.0)", true, border, mapper, "cyan");
    test_point<Point>("POINT(2.5 1.0)", true, border, mapper, "cyan");
}

template <typename Point>
void test_diamond_point_on_piece_a()
{
    typedef bg::model::ring<Point> ring_type;
    typedef bg::detail::buffer::piece_border<ring_type, Point> border_type;

    ring_type offsetted, original;
    border_type border = setup_piece_border<border_type>(offsetted, original,
            diamond_offsetted, diamond_original, 'a');

#ifdef TEST_WITH_SVG
    std::ofstream svg("/tmp/border_diamond_a.svg");
    bg::svg_mapper<Point> mapper(svg, 500, 500);
    start_svg(mapper, original, offsetted, border);
#else
    int mapper = 0;
#endif

    // Points inside
    test_point<Point>("POINT(2 4)", true, border, mapper, "red");

    // Points outside
    test_point<Point>("POINT(1 3)", false, border, mapper, "green");
    test_point<Point>("POINT(1 5)", false, border, mapper, "green");
    test_point<Point>("POINT(3 3)", false, border, mapper, "green");
    test_point<Point>("POINT(3 5)", false, border, mapper, "green");

    // Points on the original (should be INSIDE)
    test_point<Point>("POINT(2 3)", true, border, mapper, "orange");
    test_point<Point>("POINT(2.5 3.5)", true, border, mapper, "orange");
    test_point<Point>("POINT(3 4)", true, border, mapper, "orange");

    // Points on the offsetted ring (should be OUTSIDE)
    test_point<Point>("POINT(1 4)", false, border, mapper, "blue");
    test_point<Point>("POINT(1.5 4.5)", false, border, mapper, "blue");
    test_point<Point>("POINT(2 5)", false, border, mapper, "blue");

    // Points on between original and offsetted ring (should be INSIDE)
    test_point<Point>("POINT(1.5 3.5)", true, border, mapper, "cyan");
    test_point<Point>("POINT(2.5 4.5)", true, border, mapper, "cyan");
}

template <typename Point>
void test_diamond_point_on_piece_c()
{
    typedef bg::model::ring<Point> ring_type;
    typedef bg::detail::buffer::piece_border<ring_type, Point> border_type;

    ring_type offsetted, original;
    border_type border = setup_piece_border<border_type>(offsetted, original,
            diamond_offsetted, diamond_original, 'c');

#ifdef TEST_WITH_SVG
    std::ofstream svg("/tmp/border_diamond_c.svg");
    bg::svg_mapper<Point> mapper(svg, 500, 500);
    start_svg(mapper, original, offsetted, border);
#else
    int mapper = 0;
#endif

    // Points inside
    test_point<Point>("POINT(4 4)", true, border, mapper, "red");

    // Points outside
    test_point<Point>("POINT(3 3)", false, border, mapper, "green");
    test_point<Point>("POINT(3 5)", false, border, mapper, "green");
    test_point<Point>("POINT(5 3)", false, border, mapper, "green");
    test_point<Point>("POINT(5 5)", false, border, mapper, "green");

    // Points on the original (should be INSIDE)
    test_point<Point>("POINT(3 4)", true, border, mapper, "orange");
    test_point<Point>("POINT(3.5 3.5)", true, border, mapper, "orange");
    test_point<Point>("POINT(4 3)", true, border, mapper, "orange");

    // Points on the offsetted ring (should be OUTSIDE)
    test_point<Point>("POINT(4 5)", false, border, mapper, "blue");
    test_point<Point>("POINT(4.5 4.5)", false, border, mapper, "blue");
    test_point<Point>("POINT(5 4)", false, border, mapper, "blue");

    // Points on between original and offsetted ring (should be INSIDE)
    test_point<Point>("POINT(3.5 4.5)", true, border, mapper, "cyan");
    test_point<Point>("POINT(4.5 3.5)", true, border, mapper, "cyan");
}

int test_main(int, char* [])
{
    BoostGeometryWriteTestConfiguration();

    typedef bg::model::point<default_test_type, 2, bg::cs::cartesian> point_type;

    test_rectangle_properties<point_type>();

    test_rectangle_point_on_piece_a<point_type>();
    test_rectangle_point_on_piece_c<point_type>();

    test_diamond_point_on_piece_a<point_type>();
    test_diamond_point_on_piece_c<point_type>();

    return 0;
}
