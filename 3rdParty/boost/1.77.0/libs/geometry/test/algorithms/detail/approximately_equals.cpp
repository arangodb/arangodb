// Boost.Geometry
// Unit Test

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/distance/point_to_geometry.hpp>
#include <boost/geometry/algorithms/detail/overlay/approximately_equals.hpp>

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

template <typename P>
auto get_distance(P const& p1, P const& p2)
{
    using namespace boost::geometry;

    using tag = typename cs_tag<P>::type;
    using strategy_type
    = typename strategy::distance::services::default_strategy
        <
            point_tag, point_tag, P, P, tag, tag
        >::type;

    strategy_type s;
    return detail::distance::point_to_point<P, P, strategy_type>::apply(p1, p2, s);
}

template <typename P, typename E>
bool test_approximately_equal(P const& p1, P const& p2, E const m,
                              bool debug)
{
    const bool result = bg::detail::overlay::approximately_equals(p1, p2, m);
    if (debug)
    {
        const auto d = get_distance(p1, p2);
        std::cout << std::boolalpha
                << " " << result
                << " " << d
                << std::endl;
    }
    return result;
}

template <typename P, typename E>
void test_all(E const multiplier, std::size_t expected_index)
{
    constexpr bool debug = false;

    if (debug)
    {
        using ct = typename bg::coordinate_type<P>::type;
        std::cout << "EPSILON: " << std::setprecision(32)
                  << std::numeric_limits<ct>::epsilon() << std::endl;
    }

    P p1;
    bg::read_wkt("POINT(1 1)", p1);
    std::size_t index = 0;
    for (double d = 1.0; d > 0; d /= 2.0, index++)
    {
        if (debug) { std::cout << index; }

        P p2 = p1;
        bg::set<0>(p2, bg::get<0>(p1) + d);

        if (test_approximately_equal(p1, p2, multiplier, debug))
        {
            BOOST_CHECK_MESSAGE(index == expected_index,
               "Expected: " << expected_index << " Detected: " << index);
            return;
        }
    }
}

int test_main(int, char* [])
{
    double m = 1000.0;
    test_all<bg::model::point<long double, 2, bg::cs::cartesian>>(m, 54);
    test_all<bg::model::point<double, 2, bg::cs::cartesian>>(m, 43);
    test_all<bg::model::point<float, 2, bg::cs::cartesian>>(m, 24);

    m *= 1000.0;
    test_all<bg::model::point<long double, 2, bg::cs::cartesian>>(m, 44);
    test_all<bg::model::point<double, 2, bg::cs::cartesian>>(m, 33);
    test_all<bg::model::point<float, 2, bg::cs::cartesian>>(m, 24);

    m *= 1000.0;
    test_all<bg::model::point<long double, 2, bg::cs::cartesian>>(m, 34);
    test_all<bg::model::point<double, 2, bg::cs::cartesian>>(m, 23);
    test_all<bg::model::point<float, 2, bg::cs::cartesian>>(m, 23);

    return 0;
}
