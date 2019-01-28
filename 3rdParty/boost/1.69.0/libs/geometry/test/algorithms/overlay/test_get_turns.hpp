// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014, 2016, 2017, 2018.
// Modifications copyright (c) 2014-2018 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_ALGORITHMS_OVERLAY_TEST_GET_TURNS_HPP
#define BOOST_GEOMETRY_TEST_ALGORITHMS_OVERLAY_TEST_GET_TURNS_HPP

#include <iostream>
#include <iomanip>

#include <geometry_test_common.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>

#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>

#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>

struct expected_pusher
    : std::vector<std::string>
{
    expected_pusher & operator()(std::string const& ex)
    {
        std::vector<std::string>::push_back(ex);
        return *this;
    }
};

inline expected_pusher expected(std::string const& ex)
{
    expected_pusher res;
    return res(ex);
}

struct equal_turn
{
    equal_turn(std::string const& s) : turn_ptr(&s) {}
    
    template <typename T>
    bool operator()(T const& t) const
    {
        std::string const& s = (*turn_ptr);
        std::string::size_type const count = s.size();

        return (count > 0
               ? bg::method_char(t.method) == s[0]
               : true)
            && (count > 1
                ? bg::operation_char(t.operations[0].operation) == s[1]
                : true)
            && (count > 2
                ? bg::operation_char(t.operations[1].operation) == s[2]
                : true)
            && equal_operations_ex(t.operations[0], t.operations[1], s);
    }

    template <typename P, typename R>
    static bool equal_operations_ex(bg::detail::overlay::turn_operation<P, R> const& /*op0*/,
                                    bg::detail::overlay::turn_operation<P, R> const& /*op1*/,
                                    std::string const& /*s*/)
    {
        return true;
    }

    template <typename P, typename R>
    static bool equal_operations_ex(bg::detail::overlay::turn_operation_linear<P, R> const& op0,
                                    bg::detail::overlay::turn_operation_linear<P, R> const& op1,
                                    std::string const& s)
    {
        std::string::size_type const count = s.size();

        return (count > 3
               ? is_colinear_char(op0.is_collinear) == s[3]
               : true)
            && (count > 4
               ? is_colinear_char(op1.is_collinear) == s[4]
               : true);
    }

    static char is_colinear_char(bool is_collinear)
    {
        return is_collinear ? '=' : '+';
    }

    const std::string * turn_ptr;
};

template <typename Turns>
struct turns_printer
{
    turns_printer(Turns const& t) : turns(t) {}

    friend std::ostream & operator<<(std::ostream & os, turns_printer const& tp)
    {
        std::vector<std::string> vec(tp.turns.size());
        std::transform(tp.turns.begin(), tp.turns.end(), vec.begin(), to_string());
        std::sort(vec.begin(), vec.end());
        std::copy(vec.begin(), vec.end(), std::ostream_iterator<std::string>(os, " "));
        return os;
    }

    struct to_string
    {
        template <typename P, typename R>
        std::string operator()(bg::detail::overlay::turn_info<P, R, bg::detail::overlay::turn_operation<P, R> > const& t) const
        {
            std::string res(3, ' ');
            res[0] = bg::method_char(t.method);
            res[1] = bg::operation_char(t.operations[0].operation);
            res[2] = bg::operation_char(t.operations[1].operation);
            return res;
        }

        template <typename P, typename R>
        std::string operator()(bg::detail::overlay::turn_info<P, R, bg::detail::overlay::turn_operation_linear<P, R> > const& t) const
        {
            std::string res(5, ' ');
            res[0] = bg::method_char(t.method);
            res[1] = bg::operation_char(t.operations[0].operation);
            res[2] = bg::operation_char(t.operations[1].operation);
            res[3] = equal_turn::is_colinear_char(t.operations[0].is_collinear);
            res[4] = equal_turn::is_colinear_char(t.operations[1].is_collinear);
            return res;
        }
    };

    Turns const& turns;
};

template <>
struct turns_printer<expected_pusher>
{
    turns_printer(expected_pusher const& t) : turns(t) {}

    friend std::ostream & operator<<(std::ostream & os, turns_printer const& tp)
    {
        std::vector<std::string> vec(tp.turns.size());
        std::copy(tp.turns.begin(), tp.turns.end(), vec.begin());
        std::sort(vec.begin(), vec.end());
        std::copy(vec.begin(), vec.end(), std::ostream_iterator<std::string>(os, " "));
        return os;
    }

    expected_pusher const& turns;
};

template <typename Geometry1, typename Geometry2, typename Expected, typename Strategy>
void check_geometry_range(Geometry1 const& g1,
                          Geometry2 const& g2,
                          std::string const& wkt1,
                          std::string const& wkt2,
                          Expected const& expected,
                          Strategy const& strategy)
{
    typedef bg::detail::no_rescale_policy robust_policy_type;
    typedef typename bg::point_type<Geometry2>::type point_type;

    typedef typename bg::segment_ratio_type
        <
            point_type, robust_policy_type
        >::type segment_ratio_type;

    typedef bg::detail::overlay::turn_info
        <
            typename bg::point_type<Geometry2>::type,
            segment_ratio_type,
            typename bg::detail::get_turns::turn_operation_type
            <
                Geometry1,
                Geometry2,
                segment_ratio_type
            >::type
        > turn_info;
    typedef bg::detail::overlay::assign_null_policy assign_policy_t;
    typedef bg::detail::get_turns::no_interrupt_policy interrupt_policy_t;

    std::vector<turn_info> detected;
    interrupt_policy_t interrupt_policy;
    robust_policy_type robust_policy;
    
    // Don't switch the geometries
    typedef bg::detail::get_turns::get_turn_info_type
        <
            Geometry1, Geometry2, assign_policy_t
        > turn_policy_t;

    bg::dispatch::get_turns
        <
            typename bg::tag<Geometry1>::type, typename bg::tag<Geometry2>::type,
            Geometry1, Geometry2, false, false,
            turn_policy_t
        >::apply(0, g1, 1, g2, strategy, robust_policy, detected, interrupt_policy);

    bool ok = boost::size(expected) == detected.size();

    BOOST_CHECK_MESSAGE(ok,
        "get_turns: " << wkt1 << " and " << wkt2
        << " -> Expected turns #: " << boost::size(expected) << " detected turns #: " << detected.size());

    if (ok)
    {
        std::vector<turn_info> turns = detected;

        for ( typename boost::range_iterator<Expected const>::type sit = boost::begin(expected) ;
              sit != boost::end(expected) ; ++sit)
        {
            typename std::vector<turn_info>::iterator
                it = std::find_if(turns.begin(), turns.end(), equal_turn(*sit));

            if ( it != turns.end() )
            {
                turns.erase(it);
            }
            else
            {
                ok = false;
                break;
            }
        }
    }

    if ( !ok )
    {
        BOOST_CHECK_MESSAGE(false,
            "get_turns: " << wkt1 << " and " << wkt2
            << " -> Expected turns: " << turns_printer<Expected>(expected)
            << "Detected turns: " << turns_printer<std::vector<turn_info> >(detected));

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "Coordinates: "
                  << typeid(typename bg::coordinate_type<Geometry1>::type).name()
                  << ", "
                  << typeid(typename bg::coordinate_type<Geometry2>::type).name()
                  << std::endl;
#endif
    }
}

template <typename Geometry1, typename Geometry2, typename Expected>
void check_geometry_range(Geometry1 const& g1,
                          Geometry2 const& g2,
                          std::string const& wkt1,
                          std::string const& wkt2,
                          Expected const& expected)
{
    typename bg::strategy::intersection::services::default_strategy
        <
            typename bg::cs_tag<Geometry1>::type
        >::type strategy;

    check_geometry_range(g1, g2, wkt1, wkt2, expected, strategy);
}

template <typename Geometry1, typename Geometry2, typename Expected, typename Strategy>
void test_geometry_range(std::string const& wkt1, std::string const& wkt2,
                         Expected const& expected, Strategy const& strategy)
{
    Geometry1 geometry1;
    Geometry2 geometry2;
    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);
    check_geometry_range(geometry1, geometry2, wkt1, wkt2, expected, strategy);
}

template <typename Geometry1, typename Geometry2, typename Expected>
void test_geometry_range(std::string const& wkt1, std::string const& wkt2,
                         Expected const& expected)
{
    Geometry1 geometry1;
    Geometry2 geometry2;
    bg::read_wkt(wkt1, geometry1);
    bg::read_wkt(wkt2, geometry2);
    check_geometry_range(geometry1, geometry2, wkt1, wkt2, expected);
}

template <typename G1, typename G2>
void test_geometry(std::string const& wkt1, std::string const& wkt2,
                   std::string const& ex0)
{
    test_geometry_range<G1, G2>(wkt1, wkt2, expected(ex0));
}

template <typename G1, typename G2>
void test_geometry(std::string const& wkt1, std::string const& wkt2,
    std::string const& ex0, std::string const& ex1)
{
    test_geometry_range<G1, G2>(wkt1, wkt2, expected(ex0)(ex1));
}

template <typename G1, typename G2>
void test_geometry(std::string const& wkt1, std::string const& wkt2,
    std::string const& ex0, std::string const& ex1, std::string const& ex2)
{
    test_geometry_range<G1, G2>(wkt1, wkt2, expected(ex0)(ex1)(ex2));
}

template <typename G1, typename G2>
void test_geometry(std::string const& wkt1, std::string const& wkt2,
                   expected_pusher const& expected)
{
    test_geometry_range<G1, G2>(wkt1, wkt2, expected);
}

template <typename G1, typename G2, typename Strategy>
void test_geometry(std::string const& wkt1, std::string const& wkt2,
                   expected_pusher const& expected,
                   Strategy const& strategy)
{
    test_geometry_range<G1, G2>(wkt1, wkt2, expected, strategy);
}

#endif // BOOST_GEOMETRY_TEST_ALGORITHMS_OVERLAY_TEST_GET_TURNS_HPP
