// Boost.Geometry
// Unit Test

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_ring.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

namespace
{

bool g_debug = false;

std::string const simplex = "POLYGON((0 2,1 2,1 1,0 1,0 2))";
std::string const case_a = "POLYGON((1 0,0 5,5 2,1 0),(2 1,3 2,1 3,2 1))";
std::string const multi = "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(1 1,2 1,2 2,1 2,1 1),(3 3,4 3,4 4,3 4,3 3)),((6 6,6 10,10 10,10 6,6 6),(7 7,8 7,8 8,7 7)))";
}

template <typename Geometry>
void test_get_ring(std::string const& case_id, std::string const& wkt,
                   bg::ring_identifier const& ring_id,
                   std::string const& expected_wkt)
{
    using tag = typename bg::tag<Geometry>::type;

    Geometry geometry;
    bg::read_wkt(wkt, geometry);

    auto const ring = bg::detail::overlay::get_ring<tag>::apply(ring_id, geometry);

    if (g_debug)
    {
        std::cout << case_id
                  << " valid: " << bg::is_valid(geometry)
                  << " area: " << bg::area(geometry)
                  << " area(ring): " << bg::area(ring)
                  << " wkt(ring): " << bg::wkt(ring)
                  << std::endl;
    }

    std::ostringstream out;
    out << bg::wkt(ring);
    std::string const detected = out.str();
    BOOST_CHECK_MESSAGE(detected == expected_wkt, "get_ring: " << case_id
                        << " expected: " << expected_wkt
                        << " detected: " << detected);
}

template <typename Geometry>
void test_segment_count_on_ring(std::string const& case_id, std::string const& wkt,
                   bg::ring_identifier const& ring_id, bg::signed_size_type expected_count)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);

    auto const detected_count = bg::detail::overlay::segment_count_on_ring(geometry, ring_id);

    BOOST_CHECK_MESSAGE(detected_count == expected_count,
                        "test_get_ring: " << case_id
                        << " expected: " << expected_count
                        << " detected: " << detected_count);
}

template <typename Geometry>
void test_segment_distance(std::string const& case_id, int line, std::string const& wkt,
                           bg::segment_identifier const& id1,
                           bg::segment_identifier const& id2,
                           bg::signed_size_type expected_distance)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);

    auto const detected_distance = bg::detail::overlay::segment_distance(geometry, id1, id2);

    BOOST_CHECK_MESSAGE(detected_distance == expected_distance,
                        "segment_distance: " << case_id << " (" << line << ")"
                        << " expected: " << expected_distance
                        << " detected: " << detected_distance);
}

template <typename Point, bool Closed>
void test_get_ring()
{
    using ring = bg::model::ring<Point, Closed>;
    using polygon = bg::model::polygon<Point, Closed>;
    using multi_polygon = bg::model::multi_polygon<polygon>;

    test_get_ring<ring>("ring_simplex", simplex, {0, -1, -1}, simplex);
    test_get_ring<polygon>("polygon_simplex", simplex, {0, -1, -1}, simplex);
    test_get_ring<polygon>("case_a_outer", case_a, {0, -1, -1}, "POLYGON((1 0,0 5,5 2,1 0))");
    test_get_ring<polygon>("case_a_0", case_a, {0, -1, 0}, "POLYGON((2 1,3 2,1 3,2 1))");
    test_get_ring<multi_polygon>("multi_0_outer", multi, {0, 0, -1}, "POLYGON((0 0,0 5,5 5,5 0,0 0))");
    test_get_ring<multi_polygon>("multi_0_0", multi, {0, 0, 0}, "POLYGON((1 1,2 1,2 2,1 2,1 1))");
    test_get_ring<multi_polygon>("multi_0_1", multi, {0, 0, 1}, "POLYGON((3 3,4 3,4 4,3 4,3 3))");
    test_get_ring<multi_polygon>("multi_1_outer", multi, {0, 1, -1}, "POLYGON((6 6,6 10,10 10,10 6,6 6))");
    test_get_ring<multi_polygon>("multi_1_1", multi, {0, 1, 0}, "POLYGON((7 7,8 7,8 8,7 7))");
}

template <typename Point, bool Closed>
void test_segment_count_on_ring()
{
    using ring = bg::model::ring<Point, true, Closed>;
    using polygon = bg::model::polygon<Point, true, Closed>;
    using multi_polygon = bg::model::multi_polygon<polygon>;

    test_segment_count_on_ring<ring>("ring_simplex", simplex, {0, -1, -1}, 4);
    test_segment_count_on_ring<polygon>("polygon_simplex", simplex, {0, -1, -1}, 4);
    test_segment_count_on_ring<polygon>("case_a_outer", case_a, {0, -1, -1}, 3);
    test_segment_count_on_ring<polygon>("case_a_0", case_a, {0, -1, 0}, 3);
    test_segment_count_on_ring<multi_polygon>("multi_0_outer", multi, {0, 0, -1}, 4);
    test_segment_count_on_ring<multi_polygon>("multi_0_0", multi, {0, 0, 0}, 4);
    test_segment_count_on_ring<multi_polygon>("multi_0_1", multi, {0, 0, 1}, 4);
    test_segment_count_on_ring<multi_polygon>("multi_1_outer", multi, {0, 1, -1}, 4);
    test_segment_count_on_ring<multi_polygon>("multi_1_1", multi, {0, 1, 0}, 3);
}

template <typename Point, bool Closed>
void test_segment_distance()
{
    using ring = bg::model::ring<Point, true, Closed>;

    std::string const case_id = "ring_simplex";

    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 0}, {0, -1, -1, 0}, 0);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 0}, {0, -1, -1, 1}, 1);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 0}, {0, -1, -1, 2}, 2);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 0}, {0, -1, -1, 3}, 3);

    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 1}, {0, -1, -1, 0}, 3);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 1}, {0, -1, -1, 1}, 0);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 1}, {0, -1, -1, 2}, 1);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 1}, {0, -1, -1, 3}, 2);

    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 2}, {0, -1, -1, 0}, 2);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 2}, {0, -1, -1, 1}, 3);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 2}, {0, -1, -1, 2}, 0);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 2}, {0, -1, -1, 3}, 1);

    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 3}, {0, -1, -1, 0}, 1);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 3}, {0, -1, -1, 1}, 2);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 3}, {0, -1, -1, 2}, 3);
    test_segment_distance<ring>(case_id, __LINE__, simplex, {0, -1, -1, 3}, {0, -1, -1, 3}, 0);
}

int test_main(int, char* [])
{
    using point = bg::model::point<default_test_type, 2, bg::cs::cartesian>;
    test_get_ring<point, true>();
    test_segment_count_on_ring<point, true>();
    test_segment_count_on_ring<point, false>();
    test_segment_distance<point, true>();
    test_segment_distance<point, false>();
    return 0;
}
