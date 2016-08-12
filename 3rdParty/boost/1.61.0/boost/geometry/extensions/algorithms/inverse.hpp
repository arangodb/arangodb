// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_ALGORITHMS_INVERSE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_INVERSE_HPP

#include <boost/geometry.hpp>

namespace boost { namespace geometry
{

// TODO: this is shared with sectionalize, move to somewhere else (assign?)
template <typename Box, typename Value>
inline void enlarge_box(Box& box, Value value)
{
    geometry::set<0, 0>(box, geometry::get<0, 0>(box) - value);
    geometry::set<0, 1>(box, geometry::get<0, 1>(box) - value);
    geometry::set<1, 0>(box, geometry::get<1, 0>(box) + value);
    geometry::set<1, 1>(box, geometry::get<1, 1>(box) + value);
}

// TODO: when this might be moved outside extensions it should of course
// input/output a Geometry, instead of a WKT
template <typename Geometry, typename Value>
inline std::string inverse(std::string const& wkt, Value margin)
{
    Geometry geometry;
    read_wkt(wkt, geometry);

    geometry::correct(geometry);

    geometry::model::box<typename point_type<Geometry>::type> env;
    geometry::envelope(geometry, env);

    // Make its envelope a bit larger
    enlarge_box(env, margin);

    Geometry env_as_polygon;
    geometry::convert(env, env_as_polygon);

    Geometry inversed_result;
    geometry::difference(env_as_polygon, geometry, inversed_result);

    std::ostringstream out;

    out << geometry::wkt(inversed_result);
    return out.str();
}

}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_INVERSE_HPP
