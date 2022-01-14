// Boost.Geometry
// Unit Test

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_clusters.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

#include <vector>
#include <map>

template <typename Turn, typename Point>
Turn make_turn(Point const& p)
{
    Turn result;
    result.point = p;
    return result;
}

template <typename Point>
void do_test(std::string const& case_id,
             std::vector<Point> const& points,
             std::size_t expected_cluster_count)
{
    using coor_type = typename bg::coordinate_type<Point>::type;
    using policy_type = bg::detail::no_rescale_policy;
    using turn_info = bg::detail::overlay::turn_info
        <
            Point,
            typename bg::detail::segment_ratio_type<Point, policy_type>::type
        >;

    using cluster_type = std::map
        <
            bg::signed_size_type,
            bg::detail::overlay::cluster_info
        >;

    std::vector<turn_info> turns;
    for (auto const& p : points)
    {
        turns.push_back(make_turn<turn_info>(p));
    }

    cluster_type clusters;
    bg::detail::overlay::get_clusters(turns, clusters, policy_type());
    BOOST_CHECK_MESSAGE(expected_cluster_count == clusters.size(),
                        "Case: " << case_id
                        << " ctype: " << string_from_type<coor_type>::name()
                        << " expected: " << expected_cluster_count
                        << " detected: " << clusters.size());
}

template <typename Point>
void test_get_clusters(typename bg::coordinate_type<Point>::type eps)
{
    do_test<Point>("no", {{1.0, 1.0}, {1.0, 2.0}}, 0);
    do_test<Point>("simplex", {{1.0, 1.0}, {1.0, 1.0}}, 1);

    // Tests below come from realtime cases
    do_test<Point>("buffer1", {{2, 1},{12, 13},{6, 5},{8, 9},{1, 1},{2, 1},{1, 13},{2, 13},{5, 5},{6, 5},{5, 9},{6, 9},{13, 1},{12, 1},{13, 13},{12, 13},{9, 5},{8, 5},{9, 9},{8, 9},{1, 12},{5, 8},{1, 2},{5, 6},{1, 12},{5, 8},{13, 2},{9, 6},{13, 2},{9, 6},{13, 12},{9, 8}},
                   8);
    do_test<Point>("buffer2", {{2.72426406871, 1.7},{2.72426406871, 4.7},{2.3, 3.12426406871},{2.3, 3.7},{2.7, 2.72426406871},{2.7, 3.7},{2.72426406871, 3.7},{2.72426406871, 1.7},{0.7, 3.12426406871},{0.7, 1.7},{1.7, 0.7},{1.7, 2.3},{1.7, 2.7},{1.3, 2.3},{1.3, 2.7},{1.7, 5.72426406871},{1.7, 5.72426406871},{1.7, 4.7},{1.7, 5},{1.7, 4.3},{3.7, 3.72426406871},{3.72426406871, 3.7},{3.7, 3.7},{3.72426406871, 1.7},{3, 1.7},{3.7, 3.7},{3.7, 5.72426406871},{4.72426406871, 4.7},{3.7, 4.72426406871},{3.7, 4.72426406871},{3.7, 4.7},{3.7, 4.7},{3.7, 4.7},{3.7, 4.7},{3.7, 5},{3.7, 5.72426406871}},
                   6);
    do_test<Point>("buffer3", {{6.41421356237, 5},{6.41421356236, 5},{6.70710678119, 5.29289321881},{6.41421356237, 5},{6, 5},{6.41421356238, 5},{7, 5},{8, 10},{8.41421356237, 10},{8, 9.58578643763},{8.41421356237, 10},{7.41421356237, 9},{7.41421356237, 9},{7, 5.58578643763},{7, 5.58578643763},{6, 5},{6, 5},{6, 5},{6, 5},{6, 5},{6, 6},{4, 6},{4, 6},{3.41421356237, 3},{3, 5},{6, 5},{5, 3},{4, 6},{4, 6},{4, 7},{4, 8},{10.9142135624, 5.5},{8, 5},{10.4142135624, 5},{8, 5},{8, 3.58578643763},{8, 5},{9.41421356237, 7},{9.41421356237, 7},{8.91421356237, 7.5},{10, 7},{8, 9},{7.41421356237, 9},{11, 7}},
                   8);

    // Border cases
    do_test<Point>("borderx_no",  {{1, 1}, {1, 2}, {1 + eps * 10, 1}}, 0);
    do_test<Point>("borderx_yes", {{1, 1}, {1, 2}, {1 + eps, 1}}, 1);
    do_test<Point>("bordery_no",  {{1, 1}, {2, 1}, {1 + eps * 10, 1}}, 0);
    do_test<Point>("bordery_yes", {{1, 1}, {2, 1}, {1 + eps, 1}}, 1);
}

int test_main(int, char* [])
{
    using fp = bg::model::point<float, 2, bg::cs::cartesian>;
    using dp = bg::model::point<double, 2, bg::cs::cartesian>;
    using ep = bg::model::point<long double, 2, bg::cs::cartesian>;
    test_get_clusters<fp>(1.0e-8f);
    test_get_clusters<dp>(1.0e-13);
    test_get_clusters<ep>(1.0e-16);
    return 0;
}
