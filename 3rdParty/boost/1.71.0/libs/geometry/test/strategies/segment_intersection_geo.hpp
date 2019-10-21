// Boost.Geometry
// Unit Test

// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// Copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_STRATEGIES_SEGMENT_INTERSECTION_GEO_HPP
#define BOOST_GEOMETRY_TEST_STRATEGIES_SEGMENT_INTERSECTION_GEO_HPP


#include "segment_intersection_sph.hpp"

#include <boost/geometry/strategies/geographic/intersection.hpp>
#include <boost/geometry/strategies/geographic/intersection_elliptic.hpp>


template <typename S, typename P>
void test_default_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                           char m, std::size_t expected_count,
                           std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                           int opposite_id = -1)
{
    typename bg::strategy::intersection::services::default_strategy
        <
            bg::geographic_tag
        >::type strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}

template <typename S, typename P>
void test_great_elliptic(std::string const& s1_wkt, std::string const& s2_wkt,
                         char m, std::size_t expected_count,
                         std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                         int opposite_id = -1)
{
    bg::strategy::intersection::great_elliptic_segments<> strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}
/*
template <typename S, typename P>
void test_experimental_elliptic(std::string const& s1_wkt, std::string const& s2_wkt,
                                char m, std::size_t expected_count,
                                std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                                int opposite_id = -1)
{
    bg::strategy::intersection::experimental_elliptic_segments<> strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}
*/
template <typename S, typename P>
void test_geodesic_vincenty(std::string const& s1_wkt, std::string const& s2_wkt,
                            char m, std::size_t expected_count,
                            std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                            int opposite_id = -1)
{
    bg::strategy::intersection::geographic_segments<bg::strategy::vincenty, 4> strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}

template <typename S, typename P>
void test_geodesic_thomas(std::string const& s1_wkt, std::string const& s2_wkt,
                          char m, std::size_t expected_count,
                          std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                          int opposite_id = -1)
{
    bg::strategy::intersection::geographic_segments<bg::strategy::thomas, 2> strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}

template <typename S, typename P>
void test_geodesic_andoyer(std::string const& s1_wkt, std::string const& s2_wkt,
                           char m, std::size_t expected_count,
                           std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                           int opposite_id = -1)
{
    bg::strategy::intersection::geographic_segments<bg::strategy::andoyer, 1> strategy;

    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}


struct strategy_base
{
    strategy_base(char m_)
        : m(m_), expected_count(0), opposite(-1)
    {}
    strategy_base(char m_, std::string const& wkt1_)
        : m(m_), expected_count(1), wkt1(wkt1_), opposite(-1)
    {}
    strategy_base(char m_, std::string const& wkt1_, std::string const& wkt2_, bool opposite_)
        : m(m_), expected_count(1), wkt1(wkt1_), wkt2(wkt2_), opposite(opposite_ ? 1 : 0)
    {}

    char m;
    std::size_t expected_count;
    std::string wkt1, wkt2;
    int opposite;
};
struct strategy_default : strategy_base
{
    strategy_default(char m)
        : strategy_base(m)
    {}
    strategy_default(char m, std::string const& wkt1)
        : strategy_base(m, wkt1)
    {}
    strategy_default(char m, std::string const& wkt1, std::string const& wkt2, bool opposite)
        : strategy_base(m, wkt1, wkt2, opposite)
    {}
};
struct geodesic_vincenty : strategy_base
{
    geodesic_vincenty(char m)
        : strategy_base(m)
    {}
    geodesic_vincenty(char m, std::string const& wkt1)
        : strategy_base(m, wkt1)
    {}
    geodesic_vincenty(char m, std::string const& wkt1, std::string const& wkt2, bool opposite)
        : strategy_base(m, wkt1, wkt2, opposite)
    {}
};
struct geodesic_thomas : strategy_base
{
    geodesic_thomas(char m)
        : strategy_base(m)
    {}
    geodesic_thomas(char m, std::string const& wkt1)
        : strategy_base(m, wkt1)
    {}
    geodesic_thomas(char m, std::string const& wkt1, std::string const& wkt2, bool opposite)
        : strategy_base(m, wkt1, wkt2, opposite)
    {}
};
struct geodesic_andoyer : strategy_base
{
    geodesic_andoyer(char m)
        : strategy_base(m)
    {}
    geodesic_andoyer(char m, std::string const& wkt1)
        : strategy_base(m, wkt1)
    {}
    geodesic_andoyer(char m, std::string const& wkt1, std::string const& wkt2, bool opposite)
        : strategy_base(m, wkt1, wkt2, opposite)
    {}
};
struct great_elliptic : strategy_base
{
    great_elliptic(char m)
        : strategy_base(m)
    {}
    great_elliptic(char m, std::string const& wkt1)
        : strategy_base(m, wkt1)
    {}
    great_elliptic(char m, std::string const& wkt1, std::string const& wkt2, bool opposite)
        : strategy_base(m, wkt1, wkt2, opposite)
    {}
};


template <typename S, typename P>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   strategy_default const& s)
{
    test_default_strategy<S, P>(s1_wkt, s2_wkt, s.m, s.expected_count, s.wkt1, s.wkt2);
}

template <typename S, typename P>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   great_elliptic const& s)
{
    test_great_elliptic<S, P>(s1_wkt, s2_wkt, s.m, s.expected_count, s.wkt1, s.wkt2);
}

template <typename S, typename P>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   geodesic_vincenty const& s)
{
    test_geodesic_vincenty<S, P>(s1_wkt, s2_wkt, s.m, s.expected_count, s.wkt1, s.wkt2);
}

template <typename S, typename P>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   geodesic_thomas const& s)
{
    test_geodesic_thomas<S, P>(s1_wkt, s2_wkt, s.m, s.expected_count, s.wkt1, s.wkt2);
}

template <typename S, typename P>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   geodesic_andoyer const& s)
{
    test_geodesic_andoyer<S, P>(s1_wkt, s2_wkt, s.m, s.expected_count, s.wkt1, s.wkt2);
}


template <typename S, typename P, typename SR1>
void test_strategies(std::string const& s1_wkt, std::string const& s2_wkt,
                     SR1 const& sr1)
{
    test_strategy<S, P>(s1_wkt, s2_wkt, sr1);
}
template <typename S, typename P, typename SR1, typename SR2>
void test_strategies(std::string const& s1_wkt, std::string const& s2_wkt,
                     SR1 const& sr1, SR2 const& sr2)
{
    test_strategy<S, P>(s1_wkt, s2_wkt, sr1);
    test_strategy<S, P>(s1_wkt, s2_wkt, sr2);
}
template <typename S, typename P, typename SR1, typename SR2, typename SR3>
void test_strategies(std::string const& s1_wkt, std::string const& s2_wkt,
                     SR1 const& sr1, SR2 const& sr2, SR3 const& sr3)
{
    test_strategy<S, P>(s1_wkt, s2_wkt, sr1);
    test_strategy<S, P>(s1_wkt, s2_wkt, sr2);
    test_strategy<S, P>(s1_wkt, s2_wkt, sr3);
}
template <typename S, typename P, typename SR1, typename SR2, typename SR3, typename SR4>
void test_strategies(std::string const& s1_wkt, std::string const& s2_wkt,
                     SR1 const& sr1, SR2 const& sr2, SR3 const& sr3, SR4 const& sr4)
{
    test_strategy<S, P>(s1_wkt, s2_wkt, sr1);
    test_strategy<S, P>(s1_wkt, s2_wkt, sr2);
    test_strategy<S, P>(s1_wkt, s2_wkt, sr3);
    test_strategy<S, P>(s1_wkt, s2_wkt, sr4);
}


template <typename S, typename P>
void test_all_strategies(std::string const& s1_wkt, std::string const& s2_wkt,
                         char m, std::string const& ip0_wkt = "")
{
    std::size_t expected_count = ip0_wkt.empty() ? 0 : 1;
    
    test_default_strategy<S, P>(s1_wkt, s2_wkt, m, expected_count, ip0_wkt);
    test_great_elliptic<S, P>(s1_wkt, s2_wkt, m, expected_count, ip0_wkt);
    //test_experimental_elliptic<S, P>(s1_wkt, s2_wkt, m, expected_count, ip0_wkt);
    test_geodesic_vincenty<S, P>(s1_wkt, s2_wkt, m, expected_count, ip0_wkt);
    test_geodesic_thomas<S, P>(s1_wkt, s2_wkt, m, expected_count, ip0_wkt);
    test_geodesic_andoyer<S, P>(s1_wkt, s2_wkt, m, expected_count, ip0_wkt);
}

template <typename S, typename P>
void test_all_strategies(std::string const& s1_wkt, std::string const& s2_wkt,
                         char m,
                         std::string const& ip0_wkt, std::string const& ip1_wkt,
                         bool opposite)
{
    int opposite_id = opposite ? 1 : 0;

    test_default_strategy<S, P>(s1_wkt, s2_wkt, m, 2, ip0_wkt, ip1_wkt, opposite_id);
    test_great_elliptic<S, P>(s1_wkt, s2_wkt, m, 2, ip0_wkt, ip1_wkt, opposite_id);
    //test_experimental_elliptic<S, P>(s1_wkt, s2_wkt, m, 2, ip0_wkt, ip1_wkt, opposite_id);
    test_geodesic_vincenty<S, P>(s1_wkt, s2_wkt, m, 2, ip0_wkt, ip1_wkt, opposite_id);
    test_geodesic_thomas<S, P>(s1_wkt, s2_wkt, m, 2, ip0_wkt, ip1_wkt, opposite_id);
    test_geodesic_andoyer<S, P>(s1_wkt, s2_wkt, m, 2, ip0_wkt, ip1_wkt, opposite_id);
}

#endif // BOOST_GEOMETRY_TEST_STRATEGIES_SEGMENT_INTERSECTION_GEO_HPP
