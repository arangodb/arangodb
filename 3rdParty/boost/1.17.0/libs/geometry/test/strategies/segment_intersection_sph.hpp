// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_STRATEGIES_SEGMENT_INTERSECTION_SPH_HPP
#define BOOST_GEOMETRY_TEST_STRATEGIES_SEGMENT_INTERSECTION_SPH_HPP


#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/equals.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/segment.hpp>

#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>

#include <boost/geometry/policies/relate/direction.hpp>
#include <boost/geometry/policies/relate/intersection_points.hpp>
#include <boost/geometry/policies/relate/tupled.hpp>

template <typename T>
bool equals_relaxed_val(T const& v1, T const& v2, T const& eps_scale)
{
    T const c1 = 1;
    T relaxed_eps = std::numeric_limits<T>::epsilon()
        * bg::math::detail::greatest(c1, bg::math::abs(v1), bg::math::abs(v2))
        * eps_scale;
    return bg::math::abs(v1 - v2) <= relaxed_eps;
}

template <typename P1, typename P2, typename T>
bool equals_relaxed(P1 const& p1, P2 const& p2, T const& eps_scale)
{
    typedef typename bg::select_coordinate_type<P1, P2>::type calc_t;
    calc_t c1 = 1;
    calc_t p10 = bg::get<0>(p1);
    calc_t p11 = bg::get<1>(p1);
    calc_t p20 = bg::get<0>(p2);
    calc_t p21 = bg::get<1>(p2);
    calc_t relaxed_eps = std::numeric_limits<calc_t>::epsilon()
        * bg::math::detail::greatest(c1, bg::math::abs(p10), bg::math::abs(p11), bg::math::abs(p20), bg::math::abs(p21))
        * eps_scale;
    calc_t diff0 = p10 - p20;
    // needed e.g. for -179.999999 - 180.0
    if (diff0 < -180)
        diff0 += 360;
    return bg::math::abs(diff0) <= relaxed_eps
        && bg::math::abs(p11 - p21) <= relaxed_eps;
}

template <typename S1, typename S2, typename Strategy, typename P>
void test_strategy_one(S1 const& s1, S2 const& s2,
                       Strategy const& strategy,
                       char m, std::size_t expected_count,
                       P const& ip0 = P(), P const& ip1 = P(),
                       int opposite_id = -1)
{
    typedef typename bg::coordinate_type<P>::type coord_t;
    typedef bg::policies::relate::segments_tupled
                <
                    bg::policies::relate::segments_intersection_points
                        <
                            bg::segment_intersection_points<P, bg::segment_ratio<coord_t> >
                        >,
                    bg::policies::relate::segments_direction
                > policy_t;

    typedef typename policy_t::return_type return_type;

    // NOTE: robust policy is currently ignored
    return_type res = strategy.apply(s1, s2, policy_t(), 0);

    size_t const res_count = boost::get<0>(res).count;
    char const res_method = boost::get<1>(res).how;

    BOOST_CHECK_MESSAGE(res_method == m,
                        "IP method: " << res_method << " different than expected: " << m
                            << " for " << bg::wkt(s1) << " and " << bg::wkt(s2));

    BOOST_CHECK_MESSAGE(res_count == expected_count,
                        "IP count: " << res_count << " different than expected: " << expected_count
                            << " for " << bg::wkt(s1) << " and " << bg::wkt(s2));

    // The EPS is scaled because during the conversion various angles may be not converted
    // to cartesian 3d the same way which results in a different intersection point
    // For more information read the info in spherical intersection strategy file.

    // Plus in geographic CS the result strongly depends on the compiler,
    // probably due to differences in FP trigonometric function implementations

    int eps_scale = 1;
    bool is_geographic = boost::is_same<typename bg::cs_tag<S1>::type, bg::geographic_tag>::value;
    if (is_geographic)
    {
        eps_scale = 100000;
    }
    else
    {
        eps_scale = res_method != 'i' ? 1 : 1000;
    }

    if (res_count > 0 && expected_count > 0)
    {
        P const& res_i0 = boost::get<0>(res).intersections[0];
        coord_t denom_a0 = boost::get<0>(res).fractions[0].robust_ra.denominator();
        coord_t denom_b0 = boost::get<0>(res).fractions[0].robust_rb.denominator();
        BOOST_CHECK_MESSAGE(equals_relaxed(res_i0, ip0, eps_scale),
                            "IP0: " << std::setprecision(16) << bg::wkt(res_i0) << " different than expected: " << bg::wkt(ip0)
                                << " for " << bg::wkt(s1) << " and " << bg::wkt(s2));
        BOOST_CHECK_MESSAGE(denom_a0 > coord_t(0),
                            "IP0 fraction A denominator: " << std::setprecision(16) << denom_a0 << " is incorrect");
        BOOST_CHECK_MESSAGE(denom_b0 > coord_t(0),
                            "IP0 fraction B denominator: " << std::setprecision(16) << denom_b0 << " is incorrect");
    }
    if (res_count > 1 && expected_count > 1)
    {
        P const& res_i1 = boost::get<0>(res).intersections[1];
        coord_t denom_a1 = boost::get<0>(res).fractions[1].robust_ra.denominator();
        coord_t denom_b1 = boost::get<0>(res).fractions[1].robust_rb.denominator();
        BOOST_CHECK_MESSAGE(equals_relaxed(res_i1, ip1, eps_scale),
                            "IP1: " << std::setprecision(16) << bg::wkt(res_i1) << " different than expected: " << bg::wkt(ip1)
                                << " for " << bg::wkt(s1) << " and " << bg::wkt(s2));
        BOOST_CHECK_MESSAGE(denom_a1 > coord_t(0),
                            "IP1 fraction A denominator: " << std::setprecision(16) << denom_a1 << " is incorrect");
        BOOST_CHECK_MESSAGE(denom_b1 > coord_t(0),
                            "IP1 fraction B denominator: " << std::setprecision(16) << denom_b1 << " is incorrect");
    }

    if (opposite_id >= 0)
    {
        bool opposite = opposite_id != 0;
        BOOST_CHECK_MESSAGE(opposite == boost::get<1>(res).opposite,
                            bg::wkt(s1) << " and " << bg::wkt(s2) << (opposite_id == 0 ? " are not " : " are ") << "opposite" );
    }
}

template <typename T>
T translated(T v, double t)
{
    v += T(t);
    if (v > 180)
        v -= 360;
    return v;
}

template <typename S1, typename S2, typename Strategy, typename P>
void test_strategy(S1 const& s1, S2 const& s2,
                   Strategy const& strategy,
                   char m, std::size_t expected_count,
                   P const& ip0 = P(), P const& ip1 = P(),
                   int opposite_id = -1)
{
    S1 s1t = s1;
    S2 s2t = s2;
    P ip0t = ip0;
    P ip1t = ip1;

#ifndef BOOST_GEOMETRY_TEST_GEO_INTERSECTION_TEST_SIMILAR
    test_strategy_one(s1t, s2t, strategy, m, expected_count, ip0t, ip1t, opposite_id);
#else
    double t = 0;
    for (int i = 0; i < 4; ++i, t += 90)
    {
        bg::set<0, 0>(s1t, translated(bg::get<0, 0>(s1), t));
        bg::set<1, 0>(s1t, translated(bg::get<1, 0>(s1), t));
        bg::set<0, 0>(s2t, translated(bg::get<0, 0>(s2), t));
        bg::set<1, 0>(s2t, translated(bg::get<1, 0>(s2), t));
        if (expected_count > 0)
            bg::set<0>(ip0t, translated(bg::get<0>(ip0), t));
        if (expected_count > 1)
            bg::set<0>(ip1t, translated(bg::get<0>(ip1), t));

        test_strategy_one(s1t, s2t, strategy, m, expected_count, ip0t, ip1t, opposite_id);
    }
#endif
}

template <typename S1, typename S2, typename P, typename Strategy>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   Strategy const& strategy,
                   char m, std::size_t expected_count,
                   std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                   int opposite_id = -1)
{
    S1 s1;
    S2 s2;
    P ip0, ip1;

    bg::read_wkt(s1_wkt, s1);
    bg::read_wkt(s2_wkt, s2);
    if (! ip0_wkt.empty())
        bg::read_wkt(ip0_wkt, ip0);
    if (!ip1_wkt.empty())
        bg::read_wkt(ip1_wkt, ip1);

    test_strategy(s1, s2, strategy, m, expected_count, ip0, ip1, opposite_id);
}

template <typename S, typename P, typename Strategy>
void test_strategy(std::string const& s1_wkt, std::string const& s2_wkt,
                   Strategy const& strategy,
                   char m, std::size_t expected_count,
                   std::string const& ip0_wkt = "", std::string const& ip1_wkt = "",
                   int opposite_id = -1)
{
    test_strategy<S, S, P>(s1_wkt, s2_wkt, strategy, m, expected_count, ip0_wkt, ip1_wkt, opposite_id);
}

#endif // BOOST_GEOMETRY_TEST_STRATEGIES_SEGMENT_INTERSECTION_SPH_HPP
