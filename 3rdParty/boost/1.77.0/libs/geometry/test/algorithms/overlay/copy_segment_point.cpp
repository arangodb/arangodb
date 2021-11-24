// Boost.Geometry
// Unit Test

// Copyright (c) 2020 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/overlay/sort_by_side.hpp>

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

template <typename Geometry, typename GetRing>
void test_basic(GetRing get_ring, std::string const& case_id, int line,
                std::string const& wkt, bg::segment_identifier const& id,
                int offset, std::size_t expected_index)
{
    using point_type = typename bg::point_type<Geometry>::type;

    Geometry geometry;
    bg::read_wkt(wkt, geometry);

    // Check the result
    auto ring = get_ring(geometry);

    point_type point;
    bg::copy_segment_point<false>(geometry, id, offset, point);

    // Sanity check
    bool const expectation_in_range = expected_index < ring.size();
    BOOST_CHECK_MESSAGE(expectation_in_range, "Expectation out of range " << expected_index);
    if (! expectation_in_range)
    {
        return;
    }

    point_type const expected_point = ring[expected_index];

    bool const equal = ! bg::disjoint(point, expected_point);

    BOOST_CHECK_MESSAGE(equal, "copy_segment_point: " << case_id
                        << " line " << line << " at index " << expected_index
                        << " not as expected: "
                        << bg::wkt(point) << " vs " << bg::wkt(expected_point));
}

template <typename Geometry, typename GetRing>
void test_geometry(std::string const& case_id, std::string const& wkt, GetRing get_ring)
{
    // Check zero offset, all segment ids
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, {0, 0, -1, 0}, 0, 0);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, {0, 0, -1, 1}, 0, 1);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, {0, 0, -1, 2}, 0, 2);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, {0, 0, -1, 3}, 0, 3);

    // Check positive offsets, it should endlessly loop around, regardless of direction or closure
    bg::segment_identifier const start{0, 0, -1, 0};
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 1, 1);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 2, 2);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 3, 3);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 4, 0);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 5, 1);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 6, 2);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, 7, 3);

    // Check negative offsets
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -1, 3);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -2, 2);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -3, 1);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -4, 0);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -5, 3);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -6, 2);
    test_basic<Geometry>(get_ring, case_id, __LINE__, wkt, start, -7, 1);
}

template <typename T,  bool Closed>
void test_all(std::string const& case_id, std::string const& wkt)
{
    using point_type = bg::model::point<T, 2, bg::cs::cartesian>;
    using polygon_type = bg::model::polygon<point_type, true, Closed>;

    test_geometry<polygon_type>(case_id, wkt, [](polygon_type const& polygon)
    {
      return bg::exterior_ring(polygon);
    });
}

template <typename T>
void test_box(std::string const& case_id, std::string const& wkt)
{
    using point_type = bg::model::point<T, 2, bg::cs::cartesian>;
    using box_type = bg::model::box<point_type>;

    test_geometry<box_type>(case_id, wkt, [](box_type const& box)
    {
        boost::array<point_type, 4> ring;
        bg::detail::assign_box_corners_oriented<false>(box, ring);
        return ring;
    });

}

void test_circular_offset()
{
    BOOST_CHECK_EQUAL(3, bg::detail::copy_segments::circular_offset(4, 0, -1));
    BOOST_CHECK_EQUAL(2, bg::detail::copy_segments::circular_offset(4, 0, -2));
    BOOST_CHECK_EQUAL(1, bg::detail::copy_segments::circular_offset(4, 0, -3));

    BOOST_CHECK_EQUAL(6, bg::detail::copy_segments::circular_offset(10, 5, 1));
    BOOST_CHECK_EQUAL(6, bg::detail::copy_segments::circular_offset(10, 5, 11));
    BOOST_CHECK_EQUAL(6, bg::detail::copy_segments::circular_offset(10, 5, 21));

    BOOST_CHECK_EQUAL(4, bg::detail::copy_segments::circular_offset(10, 5, -1));
    BOOST_CHECK_EQUAL(4, bg::detail::copy_segments::circular_offset(10, 5, -11));
    BOOST_CHECK_EQUAL(4, bg::detail::copy_segments::circular_offset(10, 5, -21));
}

int test_main(int, char* [])
{
    test_circular_offset();

    test_all<double, true>("closed", "POLYGON((0 2,1 2,1 1,0 1,0 2))");
    test_all<double, false>("open", "POLYGON((0 2,1 2,1 1,0 1))");
    test_box<double>("box", "BOX(0 0,5 5)");
    return 0;
}
