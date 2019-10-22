// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015-2019.
// Modifications copyright (c) 2015-2019, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_SPHERICAL_ENVELOPE_BOX_HPP
#define BOOST_GEOMETRY_STRATEGIES_SPHERICAL_ENVELOPE_BOX_HPP


#include <boost/geometry/strategies/spherical/expand_box.hpp>

#include <boost/geometry/strategies/envelope.hpp>


namespace boost { namespace geometry
{

namespace strategy { namespace envelope
{


struct spherical_box
    : geometry::detail::envelope::envelope_box_on_spheroid
{
    typedef strategy::expand::spherical_box box_expand_strategy_type;

    static inline box_expand_strategy_type get_box_expand_strategy()
    {
        return box_expand_strategy_type();
    }
};


#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS

namespace services
{

template <typename CalculationType>
struct default_strategy<box_tag, spherical_equatorial_tag, CalculationType>
{
    typedef strategy::envelope::spherical_box type;
};

template <typename CalculationType>
struct default_strategy<box_tag, spherical_polar_tag, CalculationType>
{
    typedef strategy::envelope::spherical_box type;
};

template <typename CalculationType>
struct default_strategy<box_tag, geographic_tag, CalculationType>
{
    typedef strategy::envelope::spherical_box type;
};

}

#endif // DOXYGEN_NO_STRATEGY_SPECIALIZATIONS


}} // namespace strategy::envelope


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_STRATEGIES_SPHERICAL_ENVELOPE_BOX_HPP
