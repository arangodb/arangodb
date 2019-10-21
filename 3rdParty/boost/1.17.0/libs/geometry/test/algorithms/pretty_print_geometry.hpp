// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Tests

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_PRETTY_PRINT_GEOMETRY_HPP
#define BOOST_GEOMETRY_TEST_PRETTY_PRINT_GEOMETRY_HPP

#include <iostream>

#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/io/dsv/write.hpp>
#include <boost/geometry/io/wkt/write.hpp>


template
<
    typename Geometry,
    typename Tag = typename boost::geometry::tag<Geometry>::type
>
struct pretty_print_geometry
{
    static inline std::ostream&
    apply(std::ostream& os, Geometry const& geometry)
    {
        os << boost::geometry::wkt(geometry);
        return os;
    }
};

template <typename Box>
struct pretty_print_geometry<Box, boost::geometry::box_tag>
{
    static inline std::ostream&
    apply(std::ostream& os, Box const& box)
    {
        return os << "BOX" << boost::geometry::dsv(box);
    }
};

template <typename Segment>
struct pretty_print_geometry<Segment, boost::geometry::segment_tag>
{
    static inline std::ostream&
    apply(std::ostream& os, Segment const& segment)
    {
        return os << "SEGMENT" << boost::geometry::dsv(segment);
    }
};

template <typename Ring>
struct pretty_print_geometry<Ring, boost::geometry::ring_tag>
{
    static inline std::ostream&
    apply(std::ostream& os, Ring const& ring)
    {
        return os << "RING" << boost::geometry::dsv(ring);
    }
};


#endif // BOOST_GEOMETRY_TEST_PRETTY_PRINT_GEOMETRY_HPP
