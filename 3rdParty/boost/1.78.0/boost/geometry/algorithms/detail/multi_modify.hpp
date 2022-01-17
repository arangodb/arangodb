// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2017-2021.
// Modifications copyright (c) 2017-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_MULTI_MODIFY_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_MULTI_MODIFY_HPP


#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{


template <typename Policy>
struct multi_modify
{
    template <typename MultiGeometry>
    static inline void apply(MultiGeometry& multi)
    {
        auto const end = boost::end(multi);
        for (auto it = boost::begin(multi); it != end; ++it)
        {
            Policy::apply(*it);
        }
    }

    template <typename MultiGeometry, typename Strategy>
    static inline void apply(MultiGeometry& multi, Strategy const& strategy)
    {
        auto const end = boost::end(multi);
        for (auto it = boost::begin(multi); it != end; ++it)
        {
            Policy::apply(*it, strategy);
        }
    }
};


} // namespace detail
#endif


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_MULTI_MODIFY_HPP
