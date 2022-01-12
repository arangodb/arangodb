// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Tests

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_FROM_WKT_HPP
#define BOOST_GEOMETRY_TEST_FROM_WKT_HPP

#include <string>

#include <boost/geometry/io/wkt/read.hpp>

template <typename Geometry>
inline Geometry from_wkt(std::string const& wkt)
{
    Geometry result;
    boost::geometry::read_wkt(wkt, result);
    return result;
}

#endif // BOOST_GEOMETRY_TEST_FROM_WKT_HPP
