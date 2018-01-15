// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#ifndef BOOST_GEOMETRY_TEST_GET_TURNS_LL_INVARIANCE_HPP
#define BOOST_GEOMETRY_TEST_GET_TURNS_LL_INVARIANCE_HPP

#include <vector>

#include <boost/geometry/algorithms/reverse.hpp>

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>

#include <boost/geometry/algorithms/detail/relate/turns.hpp>

#include <boost/geometry/algorithms/detail/turns/compare_turns.hpp>
#include <boost/geometry/algorithms/detail/turns/print_turns.hpp>
#include <boost/geometry/algorithms/detail/turns/filter_continue_turns.hpp>
#include <boost/geometry/algorithms/detail/turns/remove_duplicate_turns.hpp>

#include <boost/geometry/io/wkt/write.hpp>


namespace bg = ::boost::geometry;
namespace bg_detail = ::boost::geometry::detail;
namespace bg_turns = bg_detail::turns;

template
<
    bool Enable = true,
    bool EnableRemoveDuplicateTurns = true,
    bool EnableDegenerateTurns = true
>
class test_get_turns_ll_invariance
{
private:
    struct assign_policy
    {
        static bool const include_no_turn = false;
        static bool const include_degenerate = EnableDegenerateTurns;
        static bool const include_opposite = false;

        template
        <
            typename Info,
            typename Point1,
            typename Point2,
            typename IntersectionInfo
        >
        static inline void apply(Info& , Point1 const& , Point2 const& ,
                                 IntersectionInfo const&)
        {
        }
    };



    template
    <
        typename Turns,
        typename LinearGeometry1,
        typename LinearGeometry2
    >
    static inline void compute_turns(Turns& turns,
                                     LinearGeometry1 const& linear1,
                                     LinearGeometry2 const& linear2)
    {
        turns.clear();
        bg_detail::relate::turns::get_turns
            <
                LinearGeometry1,
                LinearGeometry2,
                bg_detail::get_turns::get_turn_info_type
                <
                    LinearGeometry1,
                    LinearGeometry2,
                    assign_policy
                >
            >::apply(turns, linear1, linear2);
    }



public:
    template <typename Linear1, typename Linear2>
    static inline void apply(Linear1 const& lineargeometry1,
                             Linear2 const& lineargeometry2)
    {
        typedef typename bg_detail::relate::turns::get_turns
            <
                Linear1, Linear2
            >::turn_info turn_info;

        typedef std::vector<turn_info> turns_container;

        typedef bg_turns::filter_continue_turns
            <
                turns_container, true
            > filter_continue_turns;

        typedef bg_turns::remove_duplicate_turns
            <
                turns_container, EnableRemoveDuplicateTurns
            > remove_duplicate_turns;

        turns_container turns;

        Linear1 linear1(lineargeometry1);
        Linear2 linear2(lineargeometry2);

        Linear2 linear2_reverse(linear2);
        boost::geometry::reverse(linear2_reverse);

        turns_container turns_all, rturns_all;
        compute_turns(turns_all, linear1, linear2);
        compute_turns(rturns_all, linear1, linear2_reverse);

        turns_container turns_wo_cont(turns_all);
        turns_container rturns_wo_cont(rturns_all);

        filter_continue_turns::apply(turns_wo_cont);
        filter_continue_turns::apply(rturns_wo_cont);

        std::sort(boost::begin(turns_all), boost::end(turns_all),
                  bg_turns::less_seg_fraction_other_op<>());

        std::sort(boost::begin(turns_wo_cont), boost::end(turns_wo_cont),
                  bg_turns::less_seg_fraction_other_op<>());

        std::sort(boost::begin(rturns_all), boost::end(rturns_all),
                  bg_turns::less_seg_fraction_other_op<std::greater<boost::geometry::signed_size_type> >());

        std::sort(boost::begin(rturns_wo_cont), boost::end(rturns_wo_cont),
                  bg_turns::less_seg_fraction_other_op<std::greater<boost::geometry::signed_size_type> >());

        remove_duplicate_turns::apply(turns_all);
        remove_duplicate_turns::apply(turns_wo_cont);
        remove_duplicate_turns::apply(rturns_all);
        remove_duplicate_turns::apply(rturns_wo_cont);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl << std::endl;
        std::cout << "### ORIGINAL TURNS ###" << std::endl;
        bg_turns::print_turns(linear1, linear2, turns_all);
        std::cout << std::endl << std::endl;
        std::cout << "### ORIGINAL REVERSE TURNS ###" << std::endl;
        bg_turns::print_turns(linear1, linear2_reverse, rturns_all);
        std::cout << std::endl << std::endl;
        std::cout << "### TURNS W/O CONTINUE TURNS ###" << std::endl;
        bg_turns::print_turns(linear1, linear2, turns_wo_cont);
        std::cout << std::endl << std::endl;
        std::cout << "### REVERSE TURNS W/O CONTINUE TURNS ###" << std::endl;
        bg_turns::print_turns(linear1, linear2_reverse, rturns_wo_cont);
        std::cout << std::endl << std::endl;
#endif

        BOOST_CHECK_MESSAGE(boost::size(turns_wo_cont) == boost::size(rturns_wo_cont),
                            "Incompatible turns count: " << boost::size(turns_wo_cont) <<
                            " and " << boost::size(rturns_wo_cont) <<
                            " for L1: " << bg::wkt(lineargeometry1) <<
                            ", L2: " << bg::wkt(lineargeometry2));
    }
};

template <bool EnableRemoveDuplicateTurns, bool EnableDegenerateTurns>
class test_get_turns_ll_invariance
<
    false, EnableRemoveDuplicateTurns, EnableDegenerateTurns
>
{
public:
    template <typename Linear1, typename Linear2>
    static inline void apply(Linear1 const&, Linear2 const&)
    {
    }
};

#endif // BOOST_GEOMETRY_TEST_GET_TURNS_LL_INVARIANCE_HPP
