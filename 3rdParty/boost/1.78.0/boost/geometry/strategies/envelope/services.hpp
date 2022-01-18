// Boost.Geometry

// Copyright (c) 2020, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_STRATEGIES_ENVELOPE_SERVICES_HPP
#define BOOST_GEOMETRY_STRATEGIES_ENVELOPE_SERVICES_HPP


#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/static_assert.hpp>


namespace boost { namespace geometry
{

namespace strategies { namespace envelope { namespace services
{

template
<
    typename Geometry,
    typename Box,
    typename CSTag = typename geometry::cs_tag<Geometry>::type
>
struct default_strategy
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this coordinate system.",
        Geometry, Box, CSTag);
};

template <typename Strategy>
struct strategy_converter
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Strategy.",
        Strategy);
};


}}} // namespace strategies::envelope::services


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_STRATEGIES_ENVELOPE_SERVICES_HPP

