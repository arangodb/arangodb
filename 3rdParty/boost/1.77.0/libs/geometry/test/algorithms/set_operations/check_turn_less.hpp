// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015-2020, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_CHECK_TURN_LESS_HPP
#define BOOST_GEOMETRY_TEST_CHECK_TURN_LESS_HPP

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/size.hpp>

#include "test_set_ops_linear_linear.hpp"


// check less functor for turns
template <typename Turns, typename Less>
inline void verify_less_for_turns(Turns turns, Less const& less)
{
    typedef typename boost::range_iterator<Turns const>::type iterator_type;

    if (boost::size(turns) < 2)
    {
        return;
    }

    iterator_type last = --boost::end(turns);
    for (iterator_type it1 = boost::begin(turns); it1 != last; ++it1)
    {
        iterator_type it2 = it1;
        ++it2;
        for (; it2 != boost::end(turns); ++it2)
        {
            if (less(*it1, *it2))
            {
                BOOST_CHECK(! less(*it2, *it1));
            }
            if (less(*it2, *it1))
            {
                BOOST_CHECK(! less(*it1, *it2));
            }
        }
    }
}


struct check_turn_less
{
    template <bool EnableDegenerateTurns = true>
    struct assign_policy
    {
        static bool const include_no_turn = false;
        static bool const include_degenerate = EnableDegenerateTurns;
        static bool const include_opposite = false;
        static bool const include_start_turn = false;

        template
        <
            typename Info,
            typename Point1,
            typename Point2,
            typename IntersectionInfo
        >
        static inline void apply(Info& , Point1 const& , Point2 const& ,
                                 IntersectionInfo const& )
        {
        }
    };

    template <typename Geometry1, typename Geometry2>
    static inline void apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2)
    {
        typedef typename bg::strategies::relate::services::default_strategy
            <
                Geometry1, Geometry2
            >::type strategy_type;

        typedef bg::detail::no_rescale_policy robust_policy_type;

        typedef bg::detail::relate::turns::get_turns
            <
                Geometry1,
                Geometry2,
                bg::detail::get_turns::get_turn_info_type
                    <
                        Geometry1, Geometry2, assign_policy<>
                    >
            > get_turns_type;

        typedef typename get_turns_type::template turn_info_type
            <
                strategy_type, robust_policy_type
            >::type turn_info;

        typedef std::vector<turn_info> turns_container;

        turns_container turns;

        bg::detail::get_turns::no_interrupt_policy interrupt_policy;

        get_turns_type::apply(turns, geometry1, geometry2,
                              interrupt_policy,
                              strategy_type(), robust_policy_type());


        typedef bg::detail::turns::less_seg_fraction_other_op<> turn_less_type;

        verify_less_for_turns(turns, turn_less_type());
    }
};

#endif // BOOST_GEOMETRY_TEST_CHECK_TURN_LESS_HPP
