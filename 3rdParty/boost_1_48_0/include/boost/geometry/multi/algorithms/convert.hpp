// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2011 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2011 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2011 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_MULTI_ALGORITHMS_CONVERT_HPP
#define BOOST_GEOMETRY_MULTI_ALGORITHMS_CONVERT_HPP


#include <boost/range/metafunctions.hpp>

#include <boost/geometry/algorithms/convert.hpp>

#include <boost/geometry/multi/core/tags.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace conversion
{

template <typename Single, typename Multi, typename Policy>
struct single_to_multi
{
    static inline void apply(Single const& single, Multi& multi)
    {
        traits::resize<Multi>::apply(multi, 1);
        Policy::apply(single, *boost::begin(multi));
    }
};



template <typename Multi1, typename Multi2, typename Policy>
struct multi_to_multi
{
    static inline void apply(Multi1 const& multi1, Multi2& multi2)
    {
        traits::resize<Multi2>::apply(multi2, boost::size(multi1));

        typename boost::range_iterator<Multi1 const>::type it1
                = boost::begin(multi1);
        typename boost::range_iterator<Multi2>::type it2
                = boost::begin(multi2);

        for (; it1 != boost::end(multi1); ++it1, ++it2)
        {
            Policy::apply(*it1, *it2);
        }
    }
};


}} // namespace detail::convert
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

// Dispatch for multi <-> multi, specifying their single-version as policy.
// Note that, even if the multi-types are mutually different, their single
// version types might be the same and therefore we call boost::is_same again

template <std::size_t DimensionCount, typename Multi1, typename Multi2>
struct convert<false, multi_tag, multi_tag, DimensionCount, Multi1, Multi2>
    : detail::conversion::multi_to_multi
        <
            Multi1, 
            Multi2,
            convert
                <
                    boost::is_same
                        <
                            typename boost::range_value<Multi1>::type, 
                            typename boost::range_value<Multi2>::type
                        >::value,
                    typename single_tag_of
                                <
                                    typename tag<Multi1>::type
                                >::type,
                    typename single_tag_of
                                <
                                    typename tag<Multi2>::type
                                >::type,
                    DimensionCount,
                    typename boost::range_value<Multi1>::type,
                    typename boost::range_value<Multi2>::type
                >
        >
{};

template <std::size_t DimensionCount, typename SingleTag, typename Single, typename Multi>
struct convert<false, SingleTag, multi_tag, DimensionCount, Single, Multi>
    : detail::conversion::single_to_multi
        <
            Single, 
            Multi,
            convert
                <
                    false,
                    typename tag<Single>::type,
                    typename single_tag_of
                                <
                                    typename tag<Multi>::type
                                >::type,
                    DimensionCount,
                    Single,
                    typename boost::range_value<Multi>::type
                >
        >
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_MULTI_ALGORITHMS_CONVERT_HPP
