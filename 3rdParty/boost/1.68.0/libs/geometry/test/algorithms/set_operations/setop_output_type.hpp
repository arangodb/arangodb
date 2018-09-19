// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_SETOP_OUTPUT_TYPE_HPP
#define BOOST_GEOMETRY_TEST_SETOP_OUTPUT_TYPE_HPP

#include <vector>

#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/core/tag.hpp>

#include <boost/geometry/geometries/geometries.hpp>

template <typename Single, typename Tag = typename bg::tag<Single>::type>
struct setop_output_type
{
    typedef std::vector<Single> type;
};

template <typename Polygon>
struct setop_output_type<Polygon, bg::polygon_tag>
{
    typedef bg::model::multi_polygon<Polygon> type;
};

template <typename Linestring>
struct setop_output_type<Linestring, bg::linestring_tag>
{
    typedef bg::model::multi_linestring<Linestring> type;
};

template <typename Point>
struct setop_output_type<Point, bg::point_tag>
{
    typedef bg::model::multi_point<Point> type;
};

#endif // BOOST_GEOMETRY_TEST_SETOP_OUTPUT_TYPE_HPP
