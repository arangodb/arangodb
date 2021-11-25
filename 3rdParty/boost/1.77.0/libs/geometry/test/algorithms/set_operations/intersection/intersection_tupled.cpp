// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2020, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/cartesian/intersection.hpp>
#include <boost/geometry/strategies/cartesian/point_in_poly_winding.hpp>
#include <boost/geometry/strategies/cartesian/point_in_point.hpp>

#include <boost/tuple/tuple.hpp>

// TEMP
#include <boost/geometry/strategies/cartesian.hpp>
#include <boost/geometry/strategies/geographic.hpp>
#include <boost/geometry/strategies/spherical.hpp>


typedef bg::model::point<double, 2, bg::cs::cartesian> Pt;
typedef bg::model::linestring<Pt> Ls;
typedef bg::model::polygon<Pt> Po;
typedef bg::model::ring<Pt> R;
typedef bg::model::multi_point<Pt> MPt;
typedef bg::model::multi_linestring<Ls> MLs;
typedef bg::model::multi_polygon<Po> MPo;

#ifdef BOOST_GEOMETRY_CXX11_TUPLE

#include <tuple>

#endif

template <typename G>
inline void check(std::string const& wkt1,
                  std::string const& wkt2,
                  G const& g,
                  std::string const& expected)
{
    G expect;
    bg::read_wkt(expected, expect);
    bg::correct(expect);
    if (! boost::empty(g) || ! boost::empty(expect))
    {
        BOOST_CHECK_MESSAGE(
            // Commented out becasue the output in reversed case may be slightly different
            //   e.g. different number of duplicated points in MultiPoint
            //boost::size(g) == boost::size(expect) &&
            bg::equals(g, expect),
            wkt1 << " x " << wkt2 << " -> " << bg::wkt(g)
                 << " different than expected: " << expected
        );
    }
}

inline void check(std::string const& wkt1,
                  std::string const& wkt2,
                  boost::tuple<MPt, MLs, MPo> const& tup,
                  std::string const& out_p_str,
                  std::string const& out_l_str,
                  std::string const& out_a_str)
{
    check(wkt1, wkt2, boost::get<0>(tup), out_p_str);
    check(wkt1, wkt2, boost::get<1>(tup), out_l_str);
    check(wkt1, wkt2, boost::get<2>(tup), out_a_str);
}

inline void check(std::string const& wkt1,
                  std::string const& wkt2,
                  std::pair<MPt, MLs> const& pair,
                  std::string const& out_p_str,
                  std::string const& out_l_str,
                  std::string const& )
{
    check(wkt1, wkt2, pair.first, out_p_str);
    check(wkt1, wkt2, pair.second, out_l_str);
}

#ifdef BOOST_GEOMETRY_CXX11_TUPLE

inline void check(std::string const& wkt1,
                  std::string const& wkt2,
                  std::tuple<MPt, MLs, MPo> const& tup,
                  std::string const& out_p_str,
                  std::string const& out_l_str,
                  std::string const& out_a_str)
{
    check(wkt1, wkt2, std::get<0>(tup), out_p_str);
    check(wkt1, wkt2, std::get<1>(tup), out_l_str);
    check(wkt1, wkt2, std::get<2>(tup), out_a_str);
}

#endif

template <typename In1, typename In2, typename Tup>
inline void test_one(std::string const& in1_str,
                     std::string const& in2_str,
                     std::string const& out_p_str = "MULTIPOINT()",
                     std::string const& out_l_str = "MULTILINESTRING()",
                     std::string const& out_a_str = "MULTIPOLYGON()")
{
    In1 in1;
    bg::read_wkt(in1_str, in1);
    bg::correct(in1);

    In2 in2;
    bg::read_wkt(in2_str, in2);
    bg::correct(in2);

    {
        Tup result;
        bg::intersection(in1, in2, result);
        check(in1_str, in2_str, result, out_p_str, out_l_str, out_a_str);
    }
    {
        Tup result;
        bg::intersection(in2, in1, result);
        check(in1_str, in2_str, result, out_p_str, out_l_str, out_a_str);
    }
}

template <typename Tup>
inline void test_pp()
{
    test_one<Pt, Pt, Tup>(
        "POINT(0 0)",
        "POINT(0 0)",
        "MULTIPOINT(0 0)");

    test_one<Pt, MPt, Tup>(
        "POINT(0 0)",
        "MULTIPOINT(0 0, 1 1)",
        "MULTIPOINT(0 0)");

    test_one<MPt, MPt, Tup>(
        "MULTIPOINT(0 0, 1 1, 2 2)",
        "MULTIPOINT(1 1, 2 2, 3 3)",
        "MULTIPOINT(1 1, 2 2)");
}

template <typename Tup>
inline void test_pl()
{
    test_one<Pt, Ls, Tup>(
        "POINT(0 0)",
        "LINESTRING(0 0, 1 1)",
        "MULTIPOINT(0 0)");

    test_one<Pt, MLs, Tup>(
        "POINT(1 1)",
        "MULTILINESTRING((0 0, 1 1),(1 1, 2 2),(4 4, 5 5))",
        "MULTIPOINT(1 1)");

    test_one<MPt, Ls, Tup>(
        "MULTIPOINT(0 0, 1 1, 2 2, 3 3)",
        "LINESTRING(0 0, 1 1)",
        "MULTIPOINT(0 0, 1 1)");

    test_one<MPt, MLs, Tup>(
        "MULTIPOINT(0 0, 1 1, 2 2, 3 3)",
        "MULTILINESTRING((0 0, 1 1),(1 1, 2 2),(4 4, 5 5))",
        "MULTIPOINT(0 0, 1 1, 2 2)");
}

template <typename Tup>
inline void test_pa()
{
    test_one<Pt, R, Tup>(
        "POINT(0 0)",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))",
        "MULTIPOINT(0 0)");

    test_one<Pt, Po, Tup>(
        "POINT(0 0)",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "MULTIPOINT(0 0)");

    test_one<Pt, MPo, Tup>(
        "POINT(0 0)",
        "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))",
        "MULTIPOINT(0 0)");

    test_one<MPt, R, Tup>(
        "MULTIPOINT(0 0, 1 1, 2 2, 3 3, 4 4, 5 5, 6 6)",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))",
        "MULTIPOINT(0 0, 1 1, 2 2, 3 3, 4 4, 5 5)");

    test_one<MPt, Po, Tup>(
        "MULTIPOINT(0 0, 1 1, 2 2, 3 3, 4 4, 5 5, 6 6)",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "MULTIPOINT(0 0, 4 4, 5 5)");

    test_one<MPt, MPo, Tup>(
        "MULTIPOINT(0 0, 0 0, 1 1, 2 2, 3 3, 4 4, 5 5, 6 6)",
        "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))",
        "MULTIPOINT(0 0, 0 0, 1 1, 2 2, 4 4, 5 5)"); // one (0 0) could be filtered out
}

template <typename Tup>
inline void test_ll()
{
    test_one<Ls, Ls, Tup>(
        "LINESTRING(0 0, 1 0, 2 1, 3 0)",
        "LINESTRING(0 0, 1 0, 3 0, 4 0)",
        "MULTIPOINT(3 0)",
        "MULTILINESTRING((0 0, 1 0))");

    test_one<Ls, MLs, Tup>(
        "LINESTRING(0 0, 1 0, 2 1, 3 0)",
        "MULTILINESTRING((0 0, 1 0, 3 0),(2 1, 2 2))",
        "MULTIPOINT(3 0, 2 1)",
        "MULTILINESTRING((0 0, 1 0))");

    test_one<MLs, MLs, Tup>(
        "MULTILINESTRING((0 0, 1 0, 2 1),(2 1, 3 0))",
        "MULTILINESTRING((0 0, 1 0, 3 0),(2 1, 2 2))",
        "MULTIPOINT(3 0, 2 1, 2 1)", // (2 1) could be filtered out
        "MULTILINESTRING((0 0, 1 0))");


    test_one<Ls, Ls, Tup>(
        "LINESTRING(0 0, 0 5, 5 5, 5 0, 0 0)",
        "LINESTRING(0 0, 0 1, 6 1, 5 2, 5 5, 5 6, 4 5, 4 7, 7 7, 7 0, 0 0)",
        "MULTIPOINT(4 5, 5 1, 0 0)", // (0 0) could be filtered out
        "MULTILINESTRING((0 0, 0 1), (5 5, 5 2), (5 0, 0 0))");

    test_one<MLs, MLs, Tup>(
        "MULTILINESTRING((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "MULTILINESTRING((0 0, 1 4, 5 4, 5 1, 4 1, 0 0),(0 0, 2 1, 2 2, 1 2, 0 0))",
        // all (0 0) could be filtered out
        "MULTIPOINT(0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0)",
        "MULTILINESTRING((5 4,5 1),(0 0,4 1),(4 4,1 4,0 0))");
}

template <typename Tup>
inline void test_la()
{
    test_one<Ls, R, Tup>(
        "LINESTRING(0 2, -4 1, 0 0, 5 0, 9 1, 5 2, 9 3, 5 5, 4 9, 4 5, 3 3, 2 5, 2 9, 0 5)",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))",
        "MULTIPOINT(0 2, 5 2, 5 5, 0 5)",
        "MULTILINESTRING((0 0, 5 0),(4 5, 3 3, 2 5))");

    test_one<Ls, Po, Tup>(
        "LINESTRING(1 4, -4 1, 0 0, 5 0, 9 1, 5 2, 9 3, 5 5, 4 9, 4 5, 3 3, 2 5, 2 9, 0 5)",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "MULTIPOINT(5 2, 5 5, 0 5)",
        "MULTILINESTRING((1 4, 0 3.4)(0 0, 5 0),(4 5, 3.5 4),(2.5 4, 2 5))");

    test_one<MLs, R, Tup>(
        "MULTILINESTRING((0 2, -4 1, 0 0, 5 0, 9 1, 5 2, 9 3, 5 5, 4 9), (4 9, 4 5, 3 3, 2 5, 2 9, 0 5))",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))",
        "MULTIPOINT(0 2, 5 2, 5 5, 0 5)",
        "MULTILINESTRING((0 0, 5 0),(4 5, 3 3, 2 5))");

    test_one<MLs, Po, Tup>(
        "MULTILINESTRING((1 4, -4 1, 0 0, 5 0, 9 1, 5 2, 9 3, 5 5, 4 9), (4 9, 4 5, 3 3, 2 5, 2 9, 0 5))",
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "MULTIPOINT(5 2, 5 5, 0 5)",
        "MULTILINESTRING((1 4, 0 3.4)(0 0, 5 0),(4 5, 3.5 4),(2.5 4, 2 5))");

    test_one<MLs, MPo, Tup>(
        "MULTILINESTRING((1 4, -4 1, 0 0, 5 0, 9 1, 5 2, 9 3, 5 5, 4 9), (4 9, 4 5, 4 4, 2 2, 2 5, 1 9, 0 5))",
        "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))",
        "MULTIPOINT(5 2, 5 5, 2 2, 0 5)",
        "MULTILINESTRING((1 4, 0 3.4)(0 0, 5 0),(4 5, 4 4),(2 4, 2 5))");


    test_one<Ls, R, Tup>(
        "LINESTRING(0 0, 0 5, 5 5, 5 0, 0 0)",
        "POLYGON((0 0, 0 1, 6 1, 5 2, 5 5, 5 6, 4 5, 4 7, 7 7, 7 0, 0 0))",
        "MULTIPOINT(4 5)",
        "MULTILINESTRING((0 0, 0 1), (5 5, 5 2), (5 1, 5 0, 0 0))");

    test_one<MLs, Po, Tup>(
        "MULTILINESTRING((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "POLYGON((0 0, 1 4, 5 4, 5 1, 4 1, 0 0),(0 0, 2 1, 2 2, 1 2, 0 0))",
        "MULTIPOINT(0 0)", // (0 0) could be filtered out
        "MULTILINESTRING((5 4, 5 1),(0 0, 4 1, 4 4, 1 4, 0 0))");
}

template <typename Tup>
inline void test_aa()
{
    test_one<R, R, Tup>(
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))",
        "POLYGON((0 0, 0 1, 6 1, 5 2, 5 5, 5 6, 4 5, 4 7, 7 7, 7 0, 0 0))",
        "MULTIPOINT(4 5)",
        "MULTILINESTRING((5 2, 5 5))",
        "MULTIPOLYGON(((0 0, 0 1, 5 1, 5 0, 0 0)))");

    test_one<R, MPo, Tup>(
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))",
        "MULTIPOLYGON(((0 0, 0 1, 6 1, 6 0, 0 0)),"
                     "((6 1, 5 2, 5 5, 5 6, 4 5, 4 7, 7 7, 7 1, 6 1)))",
        "MULTIPOINT(4 5)",
        "MULTILINESTRING((5 2, 5 5))",
        "MULTIPOLYGON(((0 0, 0 1, 5 1, 5 0, 0 0)))");

    test_one<Po, Po, Tup>(
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "POLYGON((0 0, 1 4, 5 4, 5 1, 4 1, 0 0),(0 0, 2 1, 2 2, 1 2, 0 0))",
        // all (0 0) could be filtered out
        "MULTIPOINT(0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0)",
        "MULTILINESTRING((0 0, 1 4, 4 4),(0 0, 4 1))",
        "MULTIPOLYGON(((4 1, 4 4, 5 4, 5 1, 4 1)))");

    test_one<Po, MPo, Tup>(
        "POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))",
        "MULTIPOLYGON(((0 0, 1 4, 5 4, 5 1, 4 1, 0 0),(0 0, 2 1, 2 2, 1 2, 0 0)),"
                     "((5 0, 5 1, 6 1, 6 4, 5 4, 3 6, 2 5, 2 7, 7 7, 7 0 5 0)))",
        // all (0 0) and (5 0) could be filtered out
        "MULTIPOINT(2 5, 5 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0)",
        "MULTILINESTRING((0 0, 1 4, 4 4),(0 0, 4 1),(5 1, 5 0))",
        "MULTIPOLYGON(((4 1, 4 4, 5 4, 5 1, 4 1)),((5 4, 4 5, 5 5, 5 4)))");

    test_one<MPo, MPo, Tup>(
        "MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                     "((2 6, 2 8, 8 8, 8 5, 7 5, 7 6, 2 6)))",
        "MULTIPOLYGON(((0 0, 1 4, 5 4, 5 1, 4 1, 0 0),(0 0, 2 1, 2 2, 1 2, 0 0)),"
                     "((5 0, 5 1, 6 1, 6 4, 5 4, 3 6, 2 5, 2 7, 7 7, 7 0 5 0)))",
        // all (0 0) and (5 0) could be filtered out
        "MULTIPOINT(2 5, 5 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0, 0 0)",
        "MULTILINESTRING((0 0, 1 4, 4 4),(0 0, 4 1),(5 1, 5 0),(7 5, 7 6))",
        "MULTIPOLYGON(((4 1, 4 4, 5 4, 5 1, 4 1)),"
                     "((5 4, 4 5, 5 5, 5 4)),"
                     "((2 6, 2 7, 7 7, 7 6, 2 6)))");
}

template <typename Tup>
inline void test_pair()
{
    test_pp<Tup>();
    test_pl<Tup>();
    test_pa<Tup>();
    test_ll<Tup>();
    test_la<Tup>();
}

template <typename Tup>
inline void test_tuple()
{
    test_pp<Tup>();
    test_pl<Tup>();
    test_pa<Tup>();
    test_ll<Tup>();
    test_la<Tup>();
    test_aa<Tup>();
}

int test_main(int, char* [])
{
    test_pair<std::pair<MPt, MLs> >();
    test_tuple<boost::tuple<MPt, MLs, MPo> >();

#ifdef BOOST_GEOMETRY_CXX11_TUPLE
    test_tuple<std::tuple<MPt, MLs, MPo> >();
#endif

    return 0;
}
