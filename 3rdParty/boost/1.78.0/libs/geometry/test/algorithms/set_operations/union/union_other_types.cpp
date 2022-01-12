// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2021 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_NO_ROBUSTNESS

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <geometry_test_common.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/union.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/util/rational.hpp>

#include <set>

enum class exclude { all, rectangular, diagonal, hard, fp };

template <typename Geometry, typename Expected>
void test_one(std::string const& case_id,
              std::string const& wkt1, std::string const& wkt2,
              bool debug,
              Expected const& expected_area,
              Expected const& expected_max = -1)
{
    using coor_t = typename bg::coordinate_type<Geometry>::type;
    Geometry g1, g2, clip;

    bg::read_wkt(wkt1, g1);
    bg::read_wkt(wkt2, g2);

    bg::correct(g1);
    bg::correct(g2);

    bg::union_(g1, g2, clip);

    auto const area = bg::area(clip);
    if (debug)
    {
      std::cout << "AREA: " << std::setprecision(64) << area
                << " expected " << expected_area
                << " types coordinate " << string_from_type<coor_t>::name()
                << " area " << typeid(decltype(area)).name()
                << " expected " << typeid(Expected).name()
                << " size " << sizeof(coor_t)
                << std::endl;
    }

    // Check areas, they always have to be specified in integer for this test
    // and therefore the checking (including a tolerance) is different
    bool const ok = expected_max == -1
                    ? bg::math::equals(area, expected_area)
                    : bg::math::larger_or_equals(area, expected_area)
                      && bg::math::smaller_or_equals(area, expected_max);
    BOOST_CHECK_MESSAGE(ok,
            "union: " << case_id
            << " area: expected: " << expected_area
            << " detected: " << area
            << " type: " << (string_from_type<coor_t>::name())
            << " (" << (typeid(coor_t).name()) << ")");
}

template <typename Point>
void test_areal(std::set<exclude> const& exclude = {}, bool debug = false)
{
    using polygon = bg::model::polygon<Point>;
    using multi_polygon = bg::model::multi_polygon<polygon>;

    // Intended tests: only 3:
    // - simple case having only horizontal/vertical lines ("rectangular")
    // - simple case on integer grid but also having diagonals ("diagonal")
    // - case going wrong for <float> ("hard")

    if (exclude.count(exclude::rectangular)
        + exclude.count(exclude::all) == 0)
    {
        test_one<multi_polygon>("case_multi_rectangular",
            case_multi_rectangular[0], case_multi_rectangular[1], debug, 33125);
    }
    if (exclude.count(exclude::diagonal)
        + exclude.count(exclude::all) == 0)
    {
        test_one<multi_polygon>("case_multi_diagonal",
            case_multi_diagonal[0], case_multi_diagonal[1], debug, 5350);
    }
    if (exclude.count(exclude::hard)
        + exclude.count(exclude::fp)
        + exclude.count(exclude::all) == 0)
    {
        test_one<multi_polygon>("case_multi_hard",
            case_multi_hard[0], case_multi_hard[1], debug, 21, 23);
    }
}

int test_main(int, char* [])
{
    namespace bm = boost::multiprecision;

    using bg::model::d2::point_xy;

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    // Standard floating point types
    test_areal<point_xy<float>>({exclude::hard});
    test_areal<point_xy<double>>({});
    test_areal<point_xy<long double>>({});

    // Standard integer types
    test_areal<point_xy<std::int16_t>>({exclude::fp});
    test_areal<point_xy<std::int32_t>>({exclude::fp});
#endif
    test_areal<point_xy<std::int64_t>>({exclude::fp});

    // Boost multi precision (integer)
    test_areal<point_xy<bm::int128_t>>({exclude::fp});
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_areal<point_xy<bm::checked_int128_t>>({exclude::fp});
#endif

    // Boost multi precision (floating point)
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_areal<point_xy<bm::number<bm::cpp_bin_float<5>>>>();
    test_areal<point_xy<bm::number<bm::cpp_bin_float<10>>>>();
    test_areal<point_xy<bm::number<bm::cpp_bin_float<50>>>>();
#endif
    test_areal<point_xy<bm::number<bm::cpp_bin_float<100>>>>();

    test_areal<point_xy<bm::number<bm::cpp_dec_float<50>>>>({});

    // Boost multi precision (rational)
    test_areal<point_xy<bm::cpp_rational>>({exclude::fp});
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_areal<point_xy<bm::checked_cpp_rational>>({exclude::fp});
#endif

    // Boost multi precision float128 wrapper, is currently NOT supported
    // and it is limited to certain compilers anyway
    // test_areal<point_xy<bm::float128>>();

    // Boost rational (tests compilation)
    // (the rectangular case is correct; other input might give wrong results)
    // The int16 version throws a <zero denominator> exception
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_areal<point_xy<boost::rational<std::int16_t>>>({exclude::all});
    test_areal<point_xy<boost::rational<std::int32_t>>>({exclude::fp});
#endif
    test_areal<point_xy<boost::rational<std::int64_t>>>({exclude::fp});

    return 0;
}
