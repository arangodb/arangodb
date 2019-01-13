// Boost.Geometry

// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_AREA_RESULT_HPP
#define BOOST_GEOMETRY_STRATEGIES_AREA_RESULT_HPP


#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/cs.hpp>

#include <boost/geometry/strategies/area.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>

#include <boost/geometry/util/select_most_precise.hpp>
#include <boost/geometry/util/select_sequence_element.hpp>


#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/variant/variant_fwd.hpp>


namespace boost { namespace geometry
{


/*!
\brief Meta-function defining return type of area function
\ingroup area
\note The return-type is defined by Geometry and Strategy
 */
template
<
    typename Geometry,
    typename Strategy = default_strategy
>
struct area_result
    : Strategy::template result_type<Geometry>
{};

template <BOOST_VARIANT_ENUM_PARAMS(typename T), typename Strategy>
struct area_result<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>, Strategy>
    : geometry::area_result
        <
            typename geometry::util::select_sequence_element
                <
                    typename boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>::types
                >::type,
            Strategy
        >
{};

template <typename Geometry>
struct area_result<Geometry, default_strategy>
    : geometry::area_result
        <
            Geometry,
            typename geometry::strategy::area::services::default_strategy
                <
                    typename cs_tag<Geometry>::type
                >::type
        >
{};

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace area
{

template <typename Curr, typename Next>
struct pred_more_precise_default_area_result
{
    typedef typename geometry::area_result<Curr, default_strategy>::type curr_result_t;
    typedef typename geometry::area_result<Next, default_strategy>::type next_result_t;

    typedef typename boost::mpl::if_c
        <
            boost::is_same
                <
                    curr_result_t,
                    typename geometry::select_most_precise
                        <
                            curr_result_t,
                            next_result_t
                        >::type
                >::value,
            Curr,
            Next
        >::type type;
};

}} // namespace detail::area
#endif //DOXYGEN_NO_DETAIL

template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct area_result<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>, default_strategy>
    : geometry::area_result
        <
            typename geometry::util::select_sequence_element
                <
                    typename boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>::types,
                    geometry::detail::area::pred_more_precise_default_area_result
                >::type,
            default_strategy
        >
{};


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_STRATEGIES_AREA_RESULT_HPP
