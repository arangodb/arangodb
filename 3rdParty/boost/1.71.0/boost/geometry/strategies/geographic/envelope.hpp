// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015, 2016, 2018, 2019.
// Modifications copyright (c) 2015-2019, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_GEOGRAPHIC_ENVELOPE_HPP
#define BOOST_GEOMETRY_STRATEGIES_GEOGRAPHIC_ENVELOPE_HPP

#include <boost/geometry/srs/spheroid.hpp>

#include <boost/geometry/strategies/geographic/envelope_segment.hpp>
#include <boost/geometry/strategies/geographic/expand_segment.hpp>
#include <boost/geometry/strategies/geographic/parameters.hpp>
#include <boost/geometry/strategies/spherical/envelope.hpp>

namespace boost { namespace geometry
{

namespace strategy { namespace envelope
{


template
<
    typename FormulaPolicy = strategy::andoyer,
    typename Spheroid = geometry::srs::spheroid<double>,
    typename CalculationType = void
>
class geographic
    : public spherical<CalculationType>
{
public:
    typedef Spheroid model_type;

    inline geographic()
        : m_spheroid()
    {}

    explicit inline geographic(Spheroid const& spheroid)
        : m_spheroid(spheroid)
    {}

    typedef geographic_segment
        <
            FormulaPolicy, Spheroid, CalculationType
        > element_envelope_strategy_type;
    inline element_envelope_strategy_type get_element_envelope_strategy() const
    {
        return element_envelope_strategy_type(m_spheroid);
    }

    typedef expand::geographic_segment
        <
            FormulaPolicy, Spheroid, CalculationType
        > element_expand_strategy_type;
    inline element_expand_strategy_type get_element_expand_strategy() const
    {
        return element_expand_strategy_type(m_spheroid);
    }

    template <typename Point1, typename Point2, typename Box>
    inline void apply(Point1 const& point1, Point2 const& point2, Box& box) const
    {
        geographic_segment
            <
                FormulaPolicy,
                Spheroid,
                CalculationType
            >(m_spheroid).apply(point1, point2, box);
    }

private:
    Spheroid m_spheroid;
};

#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS

namespace services
{

template <typename Tag, typename CalculationType>
struct default_strategy<Tag, geographic_tag, CalculationType>
{
    typedef strategy::envelope::geographic
        <
            strategy::andoyer,
            geometry::srs::spheroid<double>,
            CalculationType
        > type;
};

}

#endif // DOXYGEN_NO_STRATEGY_SPECIALIZATIONS


}} // namespace strategy::envelope

}} //namepsace boost::geometry

#endif // BOOST_GEOMETRY_STRATEGIES_GEOGRAPHIC_ENVELOPE_HPP
