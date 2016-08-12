// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_expand_on_spheroid
#endif

#include <boost/test/included/unit_test.hpp>

#include <cstddef>

#include <iostream>
#include <string>

#include <geometry_test_common.hpp>
#include <from_wkt.hpp>

#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/views/detail/indexed_point_view.hpp>

#include <boost/geometry/util/condition.hpp>

#include <boost/geometry/io/dsv/write.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/expand.hpp>
#include <boost/geometry/algorithms/transform.hpp>

#include "test_envelope_expand_on_spheroid.hpp"


class test_expand_on_spheroid
{
private:
    template
    <
        typename Geometry,
        typename Tag = typename bg::tag<Geometry>::type
    >
    struct write_geometry
    {
        template <typename OutputStream>
        static inline OutputStream& apply(OutputStream& os,
                                          Geometry const& geometry)
        {
            os << bg::wkt(geometry);
            return os;
        }
    };

    template <typename Segment>
    struct write_geometry<Segment, bg::segment_tag>
    {
        template <typename OutputStream>
        static inline OutputStream& apply(OutputStream& os,
                                          Segment const& segment)
        {
            os << "SEGMENT" << bg::dsv(segment);
            return os;
        }
    };

    template <typename Box>
    struct write_geometry<Box, bg::box_tag>
    {
        template <typename OutputStream>
        static inline OutputStream& apply(OutputStream& os,
                                          Box const& box)
        {
            os << "BOX" << bg::dsv(box);
            return os;
        }
    };

    template <typename Box, typename Geometry>
    static inline void check_message(bool same_boxes,
                                     std::string const& case_id,
                                     std::string const& units_str,
                                     Box const& box,
                                     Geometry const& geometry,
                                     bool expected_are_different,
                                     Box const& expected1,
                                     Box const& expected2,
                                     Box const& detected)
    {
        std::ostringstream stream;
        stream << "case ID: " << case_id << ", "
               << "MBR units: " << units_str << "; "
               << "input box: BOX" << bg::dsv(box) << ", "
               << "geometry: ";

        write_geometry<Geometry>::apply(stream, geometry);
        stream << "; " << "expected: " << bg::dsv(expected1);

        if (expected_are_different)
        {
            stream << " or: " << bg::dsv(expected2);
        }
        stream << ", " << "detected: " << bg::dsv(detected);

        BOOST_CHECK_MESSAGE(same_boxes, stream.str());
    }


    template <bool Reverse = false, typename = void>
    struct basic_tester
    {
        template <typename Box, typename Geometry>
        static inline void base_test(std::string const& case_id,
                                     Box const& box,
                                     Geometry const& geometry,
                                     double lon_min1, double lat_min1,
                                     double height_min1,
                                     double lon_max1, double lat_max1,
                                     double height_max1,
                                     double lon_min2, double lat_min2,
                                     double height_min2,
                                     double lon_max2, double lat_max2,
                                     double height_max2,
                                     double tolerance)
        {
            typedef typename bg::coordinate_system
                <
                    Box
                >::type::units box_units_type;

            std::string const units_str = units2string<box_units_type>();

            Box detected;
            bg::convert(box, detected);
            bg::expand(detected, geometry);

            bool expected_are_different =
                (lon_min1 != lon_min2) || (lat_min1 != lat_min2)
                || (lon_max1 != lon_max2) || (lat_max1 != lat_max2);

            Box expected1;
            initialize_box<Box>::apply(expected1,
                                       lon_min1, lat_min1, height_min1,
                                       lon_max1, lat_max1, height_max1);

            Box expected2;
            initialize_box<Box>::apply(expected2,
                                       lon_min2, lat_min2, height_min2,
                                       lon_max2, lat_max2, height_max2);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "input box: BOX" << bg::dsv(box) << std::endl;

            std::cout << "geometry: ";
            write_geometry<Geometry>::apply(std::cout, geometry);

            std::cout << std::endl
                      << "MBR units: " << units_str
                      << std::endl
                      << "expected: " << bg::dsv(expected1);

            if (expected_are_different)
            {
                std::cout << " or: " << bg::dsv(expected2);
            }

            std::cout << std::endl
                      << "detected: " << bg::dsv(detected)
                      << std::endl << std::endl;
#endif
            bool same_boxes
                = box_equals<Box>::apply(detected, expected1, tolerance);

            if (expected_are_different)
            {
                same_boxes = same_boxes
                    || box_equals<Box>::apply(detected, expected2, tolerance);
            }

            check_message(same_boxes, case_id, units_str,
                          box, geometry, expected_are_different,
                          expected1, expected2, detected);
        }

        template <typename Box, typename Geometry>
        static inline void apply(std::string const& case_id,
                                 Box const& box,
                                 Geometry const& geometry,
                                 double lon_min1, double lat_min1,
                                 double height_min1,
                                 double lon_max1, double lat_max1,
                                 double height_max1,
                                 double lon_min2, double lat_min2,
                                 double height_min2,
                                 double lon_max2, double lat_max2,
                                 double height_max2,
                                 double tolerance)
        {
            typedef other_system_info
                <
                    typename bg::coordinate_system<Box>::type
                > other;

            typedef bg::model::box
                <
                    bg::model::point
                        <
                            typename bg::coordinate_type<Box>::type,
                            bg::dimension<Box>::value,
                            typename other::type
                        >
                > other_mbr_type;

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "case ID: " << case_id
                      << std::endl << std::endl;
#endif

            base_test(case_id, box, geometry,
                      lon_min1, lat_min1, height_min1,
                      lon_max1, lat_max1, height_max1,
                      lon_min2, lat_min2, height_min2,
                      lon_max2, lat_max2, height_max2,
                      tolerance);

            other_mbr_type other_box;
            bg::detail::indexed_point_view<Box const, 0> p_min(box);
            bg::detail::indexed_point_view<Box const, 1> p_max(box);
            bg::detail::indexed_point_view
                <
                    other_mbr_type, 0
                > other_min(other_box);

            bg::detail::indexed_point_view
                <
                    other_mbr_type, 1
                > other_max(other_box);

            bg::transform(p_min, other_min);
            bg::transform(p_max, other_max);

            base_test(case_id, other_box, geometry,
                      other::convert(lon_min1),
                      other::convert(lat_min1),
                      height_min1,
                      other::convert(lon_max1),
                      other::convert(lat_max1),
                      height_max1,
                      other::convert(lon_min2),
                      other::convert(lat_min2),
                      height_min2,
                      other::convert(lon_max2),
                      other::convert(lat_max2),
                      height_max2,
                      tolerance);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "=================="
                      << std::endl << std::endl;
#endif
        }
    };

    template <typename Dummy>
    struct basic_tester<true, Dummy>
    {
        template <typename Box, typename Geometry>
        static inline void apply(std::string const& case_id,
                                 Box const& box,
                                 Geometry const& geometry,
                                 double lon_min1, double lat_min1,
                                 double height_min1,
                                 double lon_max1, double lat_max1,
                                 double height_max1,
                                 double lon_min2, double lat_min2,
                                 double height_min2,
                                 double lon_max2, double lat_max2,
                                 double height_max2,
                                 double tolerance)
        {
            basic_tester
                <
                    false
                >::apply(case_id, box, geometry,
                         lon_min1, lat_min1, height_min1,
                         lon_max1, lat_max1, height_max1,
                         lon_min2, lat_min2, height_min1,
                         lon_max2, lat_max2, height_max2,
                         tolerance);

            std::string case_id_r = case_id + "[R]";

            basic_tester
                <
                    false
                >::apply(case_id_r, geometry, box,
                         lon_min1, lat_min1, height_min1,
                         lon_max1, lat_max1, height_max1,
                         lon_min2, lat_min2, height_min2,
                         lon_max2, lat_max2, height_max2,
                         tolerance);
        }
    };


public:
    template <typename Box, typename Geometry>
    static inline void apply(std::string const& case_id,
        Box const& box,
        Geometry const& geometry,
        double lon_min1, double lat_min1, double height_min1,
        double lon_max1, double lat_max1, double height_max1,
        double lon_min2, double lat_min2, double height_min2,
        double lon_max2, double lat_max2, double height_max2,
        double tolerance = std::numeric_limits<double>::epsilon())
    {

        basic_tester
            <
                boost::is_same
                    <
                        typename bg::tag<Geometry>::type,
                        bg::box_tag
                    >::value
            >::apply(case_id, box, geometry,
                     lon_min1, lat_min1, height_min1,
                     lon_max1, lat_max1, height_max1,
                     lon_min2, lat_min2, height_min2,
                     lon_max2, lat_max2, height_max2,
                     tolerance);
    }

    template <typename Box, typename Geometry>
    static inline void apply(std::string const& case_id,
        Box const& box,
        Geometry const& geometry,
        double lon_min1, double lat_min1,
        double lon_max1, double lat_max1,
        double lon_min2, double lat_min2,
        double lon_max2, double lat_max2,
        double tolerance = std::numeric_limits<double>::epsilon())
    {
        apply(case_id, box, geometry,
              lon_min1, lat_min1, 0, lon_max1, lat_max1, 0,
              lon_min2, lat_min2, 0, lon_max2, lat_max2, 0,
              tolerance);
    }

    template <typename Box, typename Geometry>
    static inline void apply(std::string const& case_id,
        Box const& box,
        Geometry const& geometry,
        double lon_min, double lat_min,
        double lon_max, double lat_max,
        double tolerance = std::numeric_limits<double>::epsilon())
    {
        apply(case_id, box, geometry,
              lon_min, lat_min, 0, lon_max, lat_max, 0,
              lon_min, lat_min, 0, lon_max, lat_max, 0,
              tolerance);
    }

    template <typename Box, typename Geometry>
    static inline void apply(std::string const& case_id,
        Box const& box,
        Geometry const& geometry,
        double lon_min, double lat_min, double height_min,
        double lon_max, double lat_max, double height_max,
        double tolerance = std::numeric_limits<double>::epsilon())
    {
        apply(case_id, box, geometry,
              lon_min, lat_min, height_min, lon_max, lat_max, height_max,
              lon_min, lat_min, height_min, lon_max, lat_max, height_max,
              tolerance);
    }
};


template <typename CoordinateSystem>
void test_expand_point()
{
    typedef bg::model::point<double, 2, CoordinateSystem> point_type;
    typedef bg::model::box<point_type> B;
    typedef point_type G;
    typedef test_expand_on_spheroid tester;

    tester::apply("p01",
                  from_wkt<B>("BOX(0 0,5 5)"),
                  from_wkt<G>("POINT(10 10)"),
                  0, 0, 10, 10);

    tester::apply("p02",
                  from_wkt<B>("BOX(0 0,5 5)"),
                  from_wkt<G>("POINT(370 10)"),
                  0, 0, 10, 10);

    tester::apply("p03",
                  from_wkt<B>("BOX(10 10,10 10)"),
                  from_wkt<G>("POINT(20 20)"),
                  10, 10, 20, 20);

    tester::apply("p04",
                  from_wkt<B>("BOX(10 10,10 10)"),
                  from_wkt<G>("POINT(10 20)"),
                  10, 10, 10, 20);

    // there are two possible valid longitude ranges:
    //   [10, 190] and [-170, 10]
    tester::apply("p05",
                  from_wkt<B>("BOX(10 10,10 10)"),
                  from_wkt<G>("POINT(190 20)"),
                  10, 10, 190, 20);

    // there are two possible valid longitude ranges:
    //   [10, 190] and [-170, 10]
    tester::apply("p05a",
                  from_wkt<B>("BOX(10 10,10 10)"),
                  from_wkt<G>("POINT(-170 20)"),
                  10, 10, 190, 20,
                  -170, 10, 10, 20);

    tester::apply("p06",
                  from_wkt<B>("BOX(170 10,175 20)"),
                  from_wkt<G>("POINT(-170 15)"),
                  170, 10, 190, 20);

    tester::apply("p06a",
                  from_wkt<B>("BOX(170 10,175 20)"),
                  from_wkt<G>("POINT(-170 -6)"),
                  170, -6, 190, 20);

    tester::apply("p06b",
                  from_wkt<B>("BOX(170 10,175 20)"),
                  from_wkt<G>("POINT(-170 36)"),
                  170, 10, 190, 36);

    // point is inside box
    tester::apply("p07",
                  from_wkt<B>("BOX(-30 -45,60 55)"),
                  from_wkt<G>("POINT(0 0)"),
                  -30, -45, 60, 55);

    // point is inside box
    tester::apply("p07a",
                  from_wkt<B>("BOX(-30 -45,60 55)"),
                  from_wkt<G>("POINT(360 0)"),
                  -30, -45, 60, 55);

    tester::apply("p08",
                  from_wkt<B>("BOX(-100 -45,-90 55)"),
                  from_wkt<G>("POINT(80 60)"),
                  -100, -45, 80, 60);

    tester::apply("p09",
                  from_wkt<B>("BOX(-100 -45,-90 55)"),
                  from_wkt<G>("POINT(170 60)"),
                  170, -45, 270, 60);

    // point is north pole
    tester::apply("p10",
                  from_wkt<B>("BOX(-100 -45,-90 55)"),
                  from_wkt<G>("POINT(-80 90)"),
                  -100, -45, -90, 90);

    // point is north pole
    tester::apply("p10a",
                  from_wkt<B>("BOX(-100 -45,-90 55)"),
                  from_wkt<G>("POINT(170 90)"),
                  -100, -45, -90, 90);

    tester::apply("p10b",
                  from_wkt<B>("BOX(-100 -45,-90 55)"),
                  from_wkt<G>("POINT(170 80)"),
                  170, -45, 270, 80);

    // box is north pole
    tester::apply("p11",
                  from_wkt<B>("BOX(10 90,20 90)"),
                  from_wkt<G>("POINT(15 89)"),
                  15, 89, 15, 90);

    // box is south pole
    tester::apply("p11a",
                  from_wkt<B>("BOX(10 -90,20 -90)"),
                  from_wkt<G>("POINT(15 89)"),
                  15, -90, 15, 89);

    // point is south pole
    tester::apply("p12",
                  from_wkt<B>("BOX(10 80,20 85)"),
                  from_wkt<G>("POINT(15 -90)"),
                  10, -90, 20, 85);

    // point is south pole
    tester::apply("p12a",
                  from_wkt<B>("BOX(10 80,20 85)"),
                  from_wkt<G>("POINT(25 -90)"),
                  10, -90, 20, 85);

    // box is north pole and point is south pole
    tester::apply("p13",
                  from_wkt<B>("BOX(10 90,20 90)"),
                  from_wkt<G>("POINT(25 -90)"),
                  0, -90, 0, 90);

    // box contains north pole and point is south pole
    tester::apply("p14",
                  from_wkt<B>("BOX(10 80,20 90)"),
                  from_wkt<G>("POINT(25 -90)"),
                  10, -90, 20, 90);

    // box contains south pole and point is north pole
    tester::apply("p15",
                  from_wkt<B>("BOX(10 -90,30 0)"),
                  from_wkt<G>("POINT(25 90)"),
                  10, -90, 30, 90);

    // box and point are north pole
    tester::apply("p15",
                  from_wkt<B>("BOX(10 90,20 90)"),
                  from_wkt<G>("POINT(25 90)"),
                  0, 90, 0, 90);

    // box and point are south pole
    tester::apply("p16",
                  from_wkt<B>("BOX(10 -90,20 -90)"),
                  from_wkt<G>("POINT(-25 -90)"),
                  0, -90, 0, -90);

    // box contains both poles
    tester::apply("p17",
                  from_wkt<B>("BOX(10 -90,10 90)"),
                  from_wkt<G>("POINT(10 80)"),
                  10, -90, 10, 90);

    // box contains both poles
    tester::apply("p17a",
                  from_wkt<B>("BOX(10 -90,10 90)"),
                  from_wkt<G>("POINT(25 80)"),
                  10, -90, 25, 90);

    // box contains both poles
    tester::apply("p18",
                  from_wkt<B>("BOX(10 -90,100 90)"),
                  from_wkt<G>("POINT(25 80)"),
                  10, -90, 100, 90);

    // box contains both poles
    tester::apply("p18a",
                  from_wkt<B>("BOX(10 -90,100 90)"),
                  from_wkt<G>("POINT(-175 80)"),
                  10, -90, 185, 90);

    // box contains both poles
    tester::apply("p18b",
                  from_wkt<B>("BOX(10 -90,100 90)"),
                  from_wkt<G>("POINT(-95 80)"),
                  -95, -90, 100, 90);

    // box contains both poles and point is north pole
    tester::apply("p19",
                  from_wkt<B>("BOX(10 -90,100 90)"),
                  from_wkt<G>("POINT(-95 90)"),
                  10, -90, 100, 90);

    // box contains both poles and point is south pole
    tester::apply("p20",
                  from_wkt<B>("BOX(10 -90,100 90)"),
                  from_wkt<G>("POINT(-95 -90)"),
                  10, -90, 100, 90);
}

BOOST_AUTO_TEST_CASE( expand_point )
{
    test_expand_point<bg::cs::spherical_equatorial<bg::degree> >();
    test_expand_point<bg::cs::geographic<bg::degree> >();
}


template <typename CoordinateSystem>
void test_expand_point_with_height()
{
    typedef bg::model::point<double, 3, CoordinateSystem> point_type;
    typedef bg::model::box<point_type> B;
    typedef point_type G;
    typedef test_expand_on_spheroid tester;

    // deactivate this for now
    tester::apply("ph01",
                  from_wkt<B>("BOX(0 0 20,5 5 100)"),
                  from_wkt<G>("POINT(10 10 80)"),
                  0, 0, 20, 10, 10, 100);

    tester::apply("ph02",
                  from_wkt<B>("BOX(0 0 20,5 5 100)"),
                  from_wkt<G>("POINT(10 10 120)"),
                  0, 0, 20, 10, 10, 120);

    tester::apply("ph03",
                  from_wkt<B>("BOX(0 0 20,5 5 100)"),
                  from_wkt<G>("POINT(10 10 5)"),
                  0, 0, 5, 10, 10, 100);
}

BOOST_AUTO_TEST_CASE( expand_point_with_height )
{
    test_expand_point_with_height<bg::cs::spherical_equatorial<bg::degree> >();
    test_expand_point_with_height<bg::cs::geographic<bg::degree> >();
}


BOOST_AUTO_TEST_CASE( expand_segment )
{
    typedef bg::cs::spherical_equatorial<bg::degree> coordinate_system_type;
    typedef bg::model::point<double, 2, coordinate_system_type> point_type;
    typedef bg::model::box<point_type> B;
    typedef bg::model::segment<point_type> G;
    typedef test_expand_on_spheroid tester;

    tester::apply("s01",
                  from_wkt<B>("BOX(20 20,50 50)"),
                  from_wkt<G>("SEGMENT(10 10,40 40)"),
                  10, 10, 50, 50);

    tester::apply("s02",
                  from_wkt<B>("BOX(20 20,50 50)"),
                  from_wkt<G>("SEGMENT(10 10,40 10)"),
                  10, 10, 50, 50);

    tester::apply("s03",
                  from_wkt<B>("BOX(5 5,50 10)"),
                  from_wkt<G>("SEGMENT(40 10,10 10)"),
                  5, 5, 50, 10.34527004614999,
                  4.0 * std::numeric_limits<double>::epsilon());

    // segment ending at the north pole
    tester::apply("s04",
                  from_wkt<B>("BOX(5 15,50 50)"),
                  from_wkt<G>("SEGMENT(40 45,80 90)"),
                  5, 15, 50, 90);

    // segment ending at the north pole
    tester::apply("s04a",
                  from_wkt<B>("BOX(5 15,30 30)"),
                  from_wkt<G>("SEGMENT(40 45,80 90)"),
                  5, 15, 40, 90);

    // segment starting at the north pole
    tester::apply("s05",
                  from_wkt<B>("BOX(5 15,50 50)"),
                  from_wkt<G>("SEGMENT(80 90,40 45)"),
                  5, 15, 50, 90);

    // segment starting at the north pole
    tester::apply("s05a",
                  from_wkt<B>("BOX(5 15,30 30)"),
                  from_wkt<G>("SEGMENT(80 90,40 45)"),
                  5, 15, 40, 90);

    // segment passing through the south pole
    tester::apply("s06",
                  from_wkt<B>("BOX(5 15,30 40)"),
                  from_wkt<G>("SEGMENT(-170 -45,10 -30)"),
                  -170, -90, 30, 40);

    // segment degenerating to the north pole
    tester::apply("s07",
                  from_wkt<B>("BOX(5 15,30 40)"),
                  from_wkt<G>("SEGMENT(10 90,20 90)"),
                  5, 15, 30, 90);

    // segment degenerating to the south pole
    tester::apply("s08",
                  from_wkt<B>("BOX(5 15,30 40)"),
                  from_wkt<G>("SEGMENT(10 -90,20 -90)"),
                  5, -90, 30, 40);

    // box degenerating to the south pole
    tester::apply("s09",
                  from_wkt<B>("BOX(10 -90,30 -90)"),
                  from_wkt<G>("SEGMENT(10 -30,100 45)"),
                  10, -90, 100, 45);

    // box degenerating to the south pole
    tester::apply("s09a",
                  from_wkt<B>("BOX(10 -90,130 -90)"),
                  from_wkt<G>("SEGMENT(10 -30,100 45)"),
                  10, -90, 100, 45);
}


BOOST_AUTO_TEST_CASE( expand_segment_with_height )
{
    typedef bg::cs::spherical_equatorial<bg::degree> coordinate_system_type;
    typedef bg::model::point<double, 3, coordinate_system_type> point_type;
    typedef bg::model::box<point_type> B;
    typedef bg::model::segment<point_type> G;
    typedef test_expand_on_spheroid tester;

    tester::apply("sh01",
                  from_wkt<B>("BOX(20 20 100,50 50 1000)"),
                  from_wkt<G>("SEGMENT(10 10 150,40 40 500)"),
                  10, 10, 100, 50, 50, 1000);

    tester::apply("sh02",
                  from_wkt<B>("BOX(20 20 100,50 50 1000)"),
                  from_wkt<G>("SEGMENT(10 10 60,40 40 1500)"),
                  10, 10, 60, 50, 50, 1500);

    tester::apply("sh03",
                  from_wkt<B>("BOX(20 20 100,50 50 1000)"),
                  from_wkt<G>("SEGMENT(10 10 150,40 40 1500)"),
                  10, 10, 100, 50, 50, 1500);

    tester::apply("sh04",
                  from_wkt<B>("BOX(20 20 100,50 50 1000)"),
                  from_wkt<G>("SEGMENT(10 10 60,40 40 800)"),
                  10, 10, 60, 50, 50, 1000);
}


template <typename CoordinateSystem>
void test_expand_box()
{
    typedef bg::model::point<double, 2, CoordinateSystem> point_type;
    typedef bg::model::box<point_type> B;
    typedef bg::model::box<point_type> G;
    typedef test_expand_on_spheroid tester;

    tester::apply("b01",
                  from_wkt<B>("BOX(11 11,19 19)"),
                  from_wkt<G>("BOX(10 10,20 20)"),
                  10, 10, 20, 20);

    tester::apply("b02",
                  from_wkt<B>("BOX(11 11,29 29)"),
                  from_wkt<G>("BOX(10 10,20 20)"),
                  10, 10, 29, 29);

    tester::apply("b03",
                  from_wkt<B>("BOX(21 21,29 29)"),
                  from_wkt<G>("BOX(10 10,20 20)"),
                  10, 10, 29, 29);

    tester::apply("b04",
                  from_wkt<B>("BOX(150 -10,200 60)"),
                  from_wkt<G>("BOX(-175 -20,-150 55)"),
                  150, -20, 210, 60);

    tester::apply("b04a",
                  from_wkt<B>("BOX(150 -10,200 60)"),
                  from_wkt<G>("BOX(-175 -20,-170 55)"),
                  150, -20, 200, 60);

    tester::apply("b04b",
                  from_wkt<B>("BOX(150 -10,200 60)"),
                  from_wkt<G>("BOX(-175 -20,-140 55)"),
                  150, -20, 220, 60);

    tester::apply("b05",
                  from_wkt<B>("BOX(150 -10,170 60)"),
                  from_wkt<G>("BOX(179 -20,535 55)"),
                  179, -20, 535, 60);

    tester::apply("b06",
                  from_wkt<B>("BOX(150 -10,170 60)"),
                  from_wkt<G>("BOX(179 -20,520 55)"),
                  179, -20, 530, 60);

    tester::apply("b07",
                  from_wkt<B>("BOX(-100 -10,170 60)"),
                  from_wkt<G>("BOX(100 -20,400 55)"),
                  -180, -20, 180, 60);

    tester::apply("b08",
                  from_wkt<B>("BOX(-100 -10,100 60)"),
                  from_wkt<G>("BOX(150 -20,150 55)"),
                  -100, -20, 150, 60);

    tester::apply("b09",
                  from_wkt<B>("BOX(100 -10,400 60)"),
                  from_wkt<G>("BOX(0 -20,0 55)"),
                  100, -20, 400, 60);

    // this is a border case:
    // there are two possible MBRs with longitude intervals:
    //               [100, 430] and [70, 400]
    tester::apply("b10",
                  from_wkt<B>("BOX(100 -10,400 60)"),
                  from_wkt<G>("BOX(70 -20,70 55)"),
                  70, -20, 400, 60,
                  100, -20, 430, 60);

    tester::apply("b10a",
                  from_wkt<B>("BOX(100 -10,400 60)"),
                  from_wkt<G>("BOX(71 -20,71 55)"),
                  71, -20, 400, 60);

    tester::apply("b10b",
                  from_wkt<B>("BOX(100 -10,400 60)"),
                  from_wkt<G>("BOX(69 -20,69 55)"),
                  100, -20, 429, 60);

    tester::apply("b11",
                  from_wkt<B>("BOX(-90 -10,-90 60)"),
                  from_wkt<G>("BOX(90 -20,90 55)"),
                  -90, -20, 90, 60);

    // first box is the south pole
    tester::apply("b12",
                  from_wkt<B>("BOX(-90 -90,80 -90)"),
                  from_wkt<G>("BOX(90 -20,90 55)"),
                  90, -90, 90, 55);

    // first box is the south pole
    tester::apply("b12a",
                  from_wkt<B>("BOX(-90 -90,80 -90)"),
                  from_wkt<G>("BOX(90 -20,130 55)"),
                  90, -90, 130, 55);

    // first box is the north pole
    tester::apply("b13",
                  from_wkt<B>("BOX(-90 90,80 90)"),
                  from_wkt<G>("BOX(90 -20,90 55)"),
                  90, -20, 90, 90);

    // first box is the north pole
    tester::apply("b13a",
                  from_wkt<B>("BOX(-90 90,80 90)"),
                  from_wkt<G>("BOX(90 -20,190 55)"),
                  90, -20, 190, 90);

    // both boxes are the north pole 
    tester::apply("b14",
                  from_wkt<B>("BOX(-90 90,80 90)"),
                  from_wkt<G>("BOX(90 90,190 90)"),
                  0, 90, 0, 90);

    // both boxes are the south pole 
    tester::apply("b15",
                  from_wkt<B>("BOX(-90 -90,80 -90)"),
                  from_wkt<G>("BOX(90 -90,190 -90)"),
                  0, -90, 0, -90);

    // one box is the south pole and the other the north pole
    tester::apply("b16",
                  from_wkt<B>("BOX(-90 -90,80 -90)"),
                  from_wkt<G>("BOX(90 90,190 90)"),
                  0, -90, 0, 90);

    // both boxes contain both poles but at different longitudes
    tester::apply("b17",
                  from_wkt<B>("BOX(10 -90,10 90)"),
                  from_wkt<G>("BOX(20 -90,20 90)"),
                  10, -90, 20, 90);

    // both boxes contain both poles but at different longitude ranges
    tester::apply("b17",
                  from_wkt<B>("BOX(10 -90,20 90)"),
                  from_wkt<G>("BOX(40 -90,60 90)"),
                  10, -90, 60, 90);


    // first box is a band
    tester::apply("b18",
                  from_wkt<B>("BOX(0 10,360 20)"),
                  from_wkt<G>("BOX(-10 -40,20 10)"),
                  -180, -40, 180, 20);

    // first box contains south and north pole and is a band
    // (box covers the entire spheroid)
    tester::apply("b19",
                  from_wkt<B>("BOX(0 -90,360 90)"),
                  from_wkt<G>("BOX(-10 -40,20 10)"),
                  -180, -90, 180, 90);

    // the envelope of the two boxes is a band
    tester::apply("b20",
                  from_wkt<B>("BOX(-180 -40,0 -30)"),
                  from_wkt<G>("BOX(0 -10,180 50)"),
                  -180, -40, 180, 50);

    // the envelope of the two boxes is a band
    tester::apply("b20a",
                  from_wkt<B>("BOX(-180 -40,0 -30)"),
                  from_wkt<G>("BOX(0 -10,185 50)"),
                  -180, -40, 180, 50);

    // the envelope of the two boxes is a band
    tester::apply("b20b",
                  from_wkt<B>("BOX(-179 -40,0 -30)"),
                  from_wkt<G>("BOX(0 -10,185 50)"),
                  -180, -40, 180, 50);
}

BOOST_AUTO_TEST_CASE( expand_box )
{
    test_expand_box<bg::cs::spherical_equatorial<bg::degree> >();
    test_expand_box<bg::cs::geographic<bg::degree> >();
}


template <typename CoordinateSystem>
void test_expand_box_with_height()
{
    typedef bg::model::point<double, 3, CoordinateSystem> point_type;
    typedef bg::model::box<point_type> B;
    typedef bg::model::box<point_type> G;
    typedef test_expand_on_spheroid tester;

    tester::apply("bh01",
                  from_wkt<B>("BOX(11 11 100,19 19 1000)"),
                  from_wkt<G>("BOX(10 10 200,20 20 800)"),
                  10, 10, 100, 20, 20, 1000);

    tester::apply("bh02",
                  from_wkt<B>("BOX(11 11 200,19 19 1000)"),
                  from_wkt<G>("BOX(10 10 100,20 20 800)"),
                  10, 10, 100, 20, 20, 1000);

    tester::apply("bh03",
                  from_wkt<B>("BOX(11 11 100,19 19 800)"),
                  from_wkt<G>("BOX(10 10 200,20 20 1000)"),
                  10, 10, 100, 20, 20, 1000);

    tester::apply("bh04",
                  from_wkt<B>("BOX(11 11 200,19 19 1000)"),
                  from_wkt<G>("BOX(10 10 100,20 20 800)"),
                  10, 10, 100, 20, 20, 1000);
}

BOOST_AUTO_TEST_CASE( expand_box_with_height )
{
    test_expand_box_with_height<bg::cs::spherical_equatorial<bg::degree> >();
    test_expand_box_with_height<bg::cs::geographic<bg::degree> >();
}
