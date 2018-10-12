// Boost.Geometry
// Unit Test

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_TEST_SRS_CHECK_GEOMETRY_HPP
#define BOOST_GEOMETRY_TEST_SRS_CHECK_GEOMETRY_HPP


#include <geometry_test_common.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/io/wkt/read.hpp>

#include <boost/geometry/views/detail/indexed_point_view.hpp>

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/size.hpp>
#include <boost/range/value_type.hpp>


namespace test
{

struct check_point
{
    template <typename Point, typename T>
    static void apply(Point const& point1, Point const& point2, T tol)
    {
        typename bg::coordinate_type<Point>::type
            x1 = bg::get<0>(point1),
            y1 = bg::get<1>(point1),
            x2 = bg::get<0>(point2),
            y2 = bg::get<1>(point2);

        BOOST_CHECK_CLOSE(x1, x2, tol);
        BOOST_CHECK_CLOSE(y1, y2, tol);
    }
};

template <typename Policy = check_point>
struct check_range
{
    template <typename Range, typename T>
    static void apply(Range const& range1, Range const& range2, T tol)
    {
        size_t range1_count = boost::size(range1);
        size_t range2_count = boost::size(range2);
        BOOST_CHECK_EQUAL(range1_count, range2_count);
        if (range1_count == range2_count)
        {
            apply(boost::begin(range1), boost::end(range1),
                  boost::begin(range2), tol);
        }
    }
    template <typename It, typename T>
    static void apply(It first1, It last1, It first2, T tol)
    {
        for ( ; first1 != last1 ; ++first1, ++first2)
            Policy::apply(*first1, *first2, tol);
    }
};


template <typename Geometry, typename Tag = typename bg::tag<Geometry>::type>
struct check_geometry_impl
{};

template <typename Point>
struct check_geometry_impl<Point, bg::point_tag>
    : check_point
{};

template <typename Segment>
struct check_geometry_impl<Segment, bg::segment_tag>
{
    template <typename T>
    static void apply(Segment const& g1, Segment const& g2, T tol)
    {
        bg::detail::indexed_point_view<Segment const, 0> p1(g1);
        bg::detail::indexed_point_view<Segment const, 1> p2(g1);
        bg::detail::indexed_point_view<Segment const, 0> q1(g2);
        bg::detail::indexed_point_view<Segment const, 1> q2(g2);

        check_point::apply(p1, q1, tol);
        check_point::apply(p2, q2, tol);
    }
};

template <typename MultiPoint>
struct check_geometry_impl<MultiPoint, bg::multi_point_tag>
    : check_range<>
{};

template <typename Linestring>
struct check_geometry_impl<Linestring, bg::linestring_tag>
    : check_range<>
{};

template <typename MultiLinestring>
struct check_geometry_impl<MultiLinestring, bg::multi_linestring_tag>
    : check_range< check_range<> >
{};

template <typename Ring>
struct check_geometry_impl<Ring, bg::ring_tag>
    : check_range<>
{};

template <typename Polygon>
struct check_geometry_impl<Polygon, bg::polygon_tag>
{
    template <typename T>
    static void apply(Polygon const& g1, Polygon const& g2, T tol)
    {
        check_range<>::apply(bg::exterior_ring(g1), bg::exterior_ring(g2), tol);
        check_range< check_range<> >::apply(bg::interior_rings(g1), bg::interior_rings(g2), tol);
    }
};

template <typename MultiPolygon>
struct check_geometry_impl<MultiPolygon, bg::multi_polygon_tag>
    : check_range
        <
            check_geometry_impl
                <
                    typename boost::range_value<MultiPolygon>::type,
                    bg::polygon_tag
                >
        >
{};


template <typename Geometry, typename T>
inline void check_geometry(Geometry const& g1, Geometry const& g2, T tol)
{
    check_geometry_impl<Geometry>::apply(g1, g2, tol);
}

template <typename Geometry, typename T>
inline void check_geometry(Geometry const& g1, std::string const& wkt2, T tol)
{
    Geometry g2;
    bg::read_wkt(wkt2, g2);
    check_geometry_impl<Geometry>::apply(g1, g2, tol);
}

} // namespace test


#endif // BOOST_GEOMETRY_TEST_SRS_CHECK_GEOMETRY_HPP
