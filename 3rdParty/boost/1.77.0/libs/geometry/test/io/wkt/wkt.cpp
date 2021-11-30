// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014-2021.
// Modifications copyright (c) 2014-2021 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <sstream>
#include <string>
#include <type_traits>

#include <boost/algorithm/string.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/topological_dimension.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>
#include <boost/variant/variant.hpp>

template <typename G>
void check_wkt(G const& geometry, std::string const& expected)
{
    std::ostringstream out;
    out << bg::wkt(geometry);
    BOOST_CHECK_EQUAL(boost::to_upper_copy(out.str()),
                      boost::to_upper_copy(expected));
}

template <typename G>
void check_to_wkt(G const& geometry, std::string const& expected)
{
    std::string out_string;
    out_string = bg::to_wkt(geometry);
    BOOST_CHECK_EQUAL(boost::to_upper_copy(out_string),
                      boost::to_upper_copy(expected));
}

template <typename G>
void check_precise_to_wkt(G const& geometry, std::string const& expected,
            int significant_digits)
{
    std::string out_string;
    out_string = bg::to_wkt(geometry, significant_digits);
    BOOST_CHECK_EQUAL(boost::to_upper_copy(out_string),
                      boost::to_upper_copy(expected));
}

template <typename G>
void test_wkt_read_write(std::string const& wkt, std::string const& expected,
              std::size_t n, double len = 0, double ar = 0, double peri = 0)
{
    G geometry;

    bg::read_wkt(wkt, geometry);

    #ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "n=" << bg::num_points(geometry)
        << " dim=" << bg::topological_dimension<G>::value
        << " length=" << bg::length(geometry)
        << " area=" << bg::area(geometry)
        << " perimeter=" << bg::perimeter(geometry)
        << std::endl << "\t\tgeometry=" << dsv(geometry)
        << std::endl;
    #endif

    BOOST_CHECK_EQUAL(bg::num_points(geometry), n);
    if (n > 0)
    {
        BOOST_CHECK_CLOSE(double(bg::length(geometry)), len, 0.0001);
        BOOST_CHECK_CLOSE(double(bg::area(geometry)), ar, 0.0001);
        BOOST_CHECK_CLOSE(double(bg::perimeter(geometry)), peri, 0.0001);
    }

    check_wkt(geometry, expected);

    boost::variant<G> v;
    bg::read_wkt(wkt, v);
    check_wkt(v, expected);

    bg::model::geometry_collection<boost::variant<G>> gc1{v};
    bg::read_wkt(std::string("GEOMETRYCOLLECTION(") + wkt + ')', gc1);
    check_wkt(gc1, std::string("GEOMETRYCOLLECTION(") + expected + ')');

    bg::model::geometry_collection<boost::variant<G>> gc2{v, v};
    bg::read_wkt(std::string("GEOMETRYCOLLECTION(") + wkt + ',' + wkt + ')', gc2);
    check_wkt(gc2, std::string("GEOMETRYCOLLECTION(") + expected + ',' + expected + ')');
}

template <typename G>
void test_wkt_to_from(std::string const& wkt, std::string const& expected,
              std::size_t n, double len = 0, double ar = 0, double peri = 0)
{
    G geometry;

    geometry = bg::from_wkt<G>(wkt);

    #ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "n=" << bg::num_points(geometry)
        << " dim=" << bg::topological_dimension<G>::value
        << " length=" << bg::length(geometry)
        << " area=" << bg::area(geometry)
        << " perimeter=" << bg::perimeter(geometry)
        << std::endl << "\t\tgeometry=" << dsv(geometry)
        << std::endl;
    #endif

    BOOST_CHECK_EQUAL(bg::num_points(geometry), n);
    if (n > 0)
    {
        BOOST_CHECK_CLOSE(double(bg::length(geometry)), len, 0.0001);
        BOOST_CHECK_CLOSE(double(bg::area(geometry)), ar, 0.0001);
        BOOST_CHECK_CLOSE(double(bg::perimeter(geometry)), peri, 0.0001);
    }

    check_to_wkt(geometry, expected);
    check_to_wkt(boost::variant<G>(geometry), expected);
}

template <typename G>
void test_wkt(std::string const& wkt, std::string const& expected,
              std::size_t n, double len = 0, double ar = 0, double peri = 0)
{
    test_wkt_read_write<G>(wkt, expected, n, len, ar, peri);
    test_wkt_to_from<G>(wkt, expected, n, len, ar, peri);
}

template <typename G>
void test_wkt(std::string const& wkt,
              std::size_t n, double len = 0, double ar = 0, double peri = 0)
{
    test_wkt<G>(wkt, wkt, n, len, ar, peri);
}

template <typename G>
void test_relaxed_wkt_read_write(std::string const& wkt, std::string const& expected)
{
    std::string e;
    G geometry;
    bg::read_wkt(wkt, geometry);
    std::ostringstream out;
    out << bg::wkt(geometry);

    BOOST_CHECK_EQUAL(boost::to_upper_copy(out.str()), boost::to_upper_copy(expected));
}

template <typename G>
void test_relaxed_wkt_to_from(std::string const& wkt, std::string const& expected)
{
    std::string e;
    G geometry;
    geometry = bg::from_wkt<G>(wkt);
    std::string out;
    out = bg::to_wkt(geometry);

    BOOST_CHECK_EQUAL(boost::to_upper_copy(out), boost::to_upper_copy(expected));
}

template <typename G>
void test_relaxed_wkt(std::string const& wkt, std::string const& expected)
{
    test_relaxed_wkt_read_write<G>(wkt, expected);
    test_relaxed_wkt_to_from<G>(wkt, expected);
}

template <typename G>
void test_wrong_wkt_read_write(std::string const& wkt, std::string const& start)
{
    std::string e("no exception");
    G geometry;
    try
    {
        bg::read_wkt<G>(wkt, geometry);
    }
    catch(bg::read_wkt_exception const& ex)
    {
        e = ex.what();
        boost::to_lower(e);
    }
    catch(...)
    {
        e = "other exception";
    }

    bool check = true;

#if defined(HAVE_TTMATH)
    // For ttmath we skip bad lexical casts
    typedef typename bg::coordinate_type<G>::type ct;

    if (boost::is_same<ct, ttmath_big>::type::value
        && boost::starts_with(start, "bad lexical cast"))
    {
        check = false;
    }
#endif

    if (check)
    {
        BOOST_CHECK_MESSAGE(boost::starts_with(e, start), "  Expected:"
                    << start << " Got:" << e << " with WKT: " << wkt);
    }
}

template <typename G>
void test_wrong_wkt_to_from(std::string const& wkt, std::string const& start)
{
    std::string e("no exception");
    G geometry;
    try
    {
        geometry = bg::from_wkt<G>(wkt);
    }
    catch(bg::read_wkt_exception const& ex)
    {
        e = ex.what();
        boost::to_lower(e);
    }
    catch(...)
    {
        e = "other exception";
    }

    BOOST_CHECK_MESSAGE(boost::starts_with(e, start), "  Expected:"
                    << start << " Got:" << e << " with WKT: " << wkt);
}

template <typename G>
void test_wrong_wkt(std::string const& wkt, std::string const& start)
{
    test_wrong_wkt_read_write<G>(wkt, start);
    test_wrong_wkt_to_from<G>(wkt, start);
}

template <typename G>
void test_wkt_output_iterator(std::string const& wkt)
{
    G geometry;
    bg::read_wkt<G>(wkt, std::back_inserter(geometry));
}

void test_precise_to_wkt()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;
    point_type point = boost::geometry::make<point_type>(1.2345, 6.7890);
    boost::geometry::model::polygon<point_type> polygon;
    boost::geometry::append(boost::geometry::exterior_ring(polygon), boost::geometry::make<point_type>(0.00000, 0.00000));
    boost::geometry::append(boost::geometry::exterior_ring(polygon), boost::geometry::make<point_type>(0.00000, 4.00001));
    boost::geometry::append(boost::geometry::exterior_ring(polygon), boost::geometry::make<point_type>(4.00001, 4.00001));
    boost::geometry::append(boost::geometry::exterior_ring(polygon), boost::geometry::make<point_type>(4.00001, 0.00000));
    boost::geometry::append(boost::geometry::exterior_ring(polygon), boost::geometry::make<point_type>(0.00000, 0.00000));
    check_precise_to_wkt(point,"POINT(1.23 6.79)",3);
    check_precise_to_wkt(polygon,"POLYGON((0 0,0 4,4 4,4 0,0 0))",3);
}

#ifndef GEOMETRY_TEST_MULTI
template <typename T>
void test_order_closure()
{
    using namespace boost::geometry;
    typedef bg::model::point<T, 2, bg::cs::cartesian> Pt;
    typedef bg::model::polygon<Pt, true, true> PCWC;
    typedef bg::model::polygon<Pt, true, false> PCWO;
    typedef bg::model::polygon<Pt, false, true> PCCWC;
    typedef bg::model::polygon<Pt, false, false> PCCWO;

    {
        std::string wkt_cwc = "POLYGON((0 0,0 2,2 2,2 0,0 0))";
        std::string wkt_cwo = "POLYGON((0 0,0 2,2 2,2 0))";
        std::string wkt_ccwc = "POLYGON((0 0,2 0,2 2,0 2,0 0))";
        std::string wkt_ccwo = "POLYGON((0 0,2 0,2 2,0 2))";

        test_wkt<PCWC>(wkt_cwc, 5, 0, 4, 8);
        test_wkt<PCWO>(wkt_cwc, 4, 0, 4, 8);
        test_wkt<PCWO>(wkt_cwo, wkt_cwc, 4, 0, 4, 8);
        test_wkt<PCCWC>(wkt_ccwc, 5, 0, 4, 8);
        test_wkt<PCCWO>(wkt_ccwc, 4, 0, 4, 8);
        test_wkt<PCCWO>(wkt_ccwo, wkt_ccwc, 4, 0, 4, 8);
    }
    {
        std::string wkt_cwc = "POLYGON((0 0,0 3,3 3,3 0,0 0),(1 1,2 1,2 2,1 2,1 1))";
        std::string wkt_cwo = "POLYGON((0 0,0 3,3 3,3 0),(1 1,2 1,2 2,1 2))";
        std::string wkt_ccwc = "POLYGON((0 0,3 0,3 3,0 3,0 0),(1 1,1 2,2 2,2 1,1 1))";
        std::string wkt_ccwo = "POLYGON((0 0,3 0,3 3,0 3),(1 1,1 2,2 2,2 1,1 1))";

        test_wkt<PCWC>(wkt_cwc, 10, 0, 8, 16);
        test_wkt<PCWO>(wkt_cwc, 8, 0, 8, 16);
        test_wkt<PCWO>(wkt_cwo, wkt_cwc, 8, 0, 8, 16);
        test_wkt<PCCWC>(wkt_ccwc, 10, 0, 8, 16);
        test_wkt<PCCWO>(wkt_ccwc, 8, 0, 8, 16);
        test_wkt<PCCWO>(wkt_ccwo, wkt_ccwc, 8, 0, 8, 16);
    }
}

template <typename T>
void test_all()
{
    using namespace boost::geometry;
    typedef bg::model::point<T, 2, bg::cs::cartesian> P;

    test_wkt<P>("POINT(1 2)", 1);
    test_wkt<bg::model::linestring<P> >("LINESTRING(1 1,2 2,3 3)", 3, 2 * sqrt(2.0));
    test_wkt<bg::model::polygon<P> >("POLYGON((0 0,0 4,4 4,4 0,0 0)"
            ",(1 1,1 2,2 2,2 1,1 1),(1 1,1 2,2 2,2 1,1 1))", 15, 0, 18, 24);

    // Non OGC: a box defined by a polygon
    //test_wkt<box<P> >("POLYGON((0 0,0 1,1 1,1 0,0 0))", 4, 0, 1, 4);
    test_wkt<bg::model::ring<P> >("POLYGON((0 0,0 1,1 1,1 0,0 0))", 5, 0, 1, 4);

    // We accept empty sequences as well (much better than EMPTY)...
    // ...or even POINT() (see below)
    test_wkt<bg::model::linestring<P> >("LINESTRING()", 0, 0);
    test_wkt<bg::model::polygon<P> >("POLYGON(())", 0);
    // ... or even with empty holes
    test_wkt<bg::model::polygon<P> >("POLYGON((),(),())", 0);
    // which all make no valid geometries, but they can exist.



    // These WKT's are incomplete or abnormal but they are considered OK
    test_relaxed_wkt<P>("POINT(1)", "POINT(1 0)");
    test_relaxed_wkt<P>("POINT()", "POINT(0 0)");
    test_relaxed_wkt<bg::model::linestring<P> >("LINESTRING(1,2,3)",
                "LINESTRING(1 0,2 0,3 0)");
    test_relaxed_wkt<P>("POINT  ( 1 2)   ", "POINT(1 2)");
    test_relaxed_wkt<P>("POINT  M ( 1 2)", "POINT(1 2)");
    test_relaxed_wkt<bg::model::box<P> >("BOX(1 1,2 2)", "POLYGON((1 1,1 2,2 2,2 1,1 1))");

    test_relaxed_wkt<bg::model::linestring<P> >("LINESTRING EMPTY", "LINESTRING()");

    test_relaxed_wkt<bg::model::polygon<P> >("POLYGON( ( ) , ( ) , ( ) )",
                "POLYGON((),(),())");

    // Wrong WKT's
    test_wrong_wkt<P>("POINT(1 2", "expected ')'");
    test_wrong_wkt<P>("POINT 1 2)", "expected '('");
    test_wrong_wkt<P>("POINT(1 2,)", "expected ')'");
    test_wrong_wkt<P>("POINT(1 2)foo", "too many tokens at 'foo'");
    test_wrong_wkt<P>("POINT(1 2 3)", "expected ')'");
    test_wrong_wkt<P>("POINT(a 2 3)", "bad lexical cast");
    test_wrong_wkt<P>("POINT 2 3", "expected '('");
    test_wrong_wkt<P>("POINT Z (1 2 3)", "z only allowed");

    test_wrong_wkt<P>("PIONT (1 2)", "should start with 'point'");

    test_wrong_wkt<bg::model::linestring<P> >("LINESTRING())", "too many tokens");

    test_wrong_wkt<bg::model::polygon<P> >("POLYGON((1 1,1 4,4 4,4 1,1 1)"
                ",((2 2,2 3,3 3,3 2,2 2))", "bad lexical cast");

    test_wrong_wkt<bg::model::box<P> >("BOX(1 1,2 2,3 3)", "box should have 2");
    test_wrong_wkt<bg::model::box<P> >("BOX(1 1,2 2) )", "too many tokens");

    if ( BOOST_GEOMETRY_CONDITION(std::is_floating_point<T>::value
                               || ! std::is_fundamental<T>::value ) )
    {
        test_wkt<P>("POINT(1.1 2.1)", 1);
    }

    // Deprecated:
    // test_wkt_output_iterator<bg::model::linestring<P> >("LINESTRING(1 1,2 2,3 3)");
    // test_wkt_output_iterator<bg::model::ring<P> >("POLYGON((1 1,2 2,3 3))");

    test_order_closure<T>();
}
#endif

int test_main(int, char* [])
{
    test_all<double>();
    test_all<int>();
    test_precise_to_wkt();

#if defined(HAVE_TTMATH)
    test_all<ttmath_big>();
#endif

    return 0;
}

/*

Results can be checked in PostGIS by query below,
or by MySQL (but replace length by glength and remove the perimeter)

Note:
- PostGIS gives "3" for a numpoints of a multi-linestring of 6 points in total (!)
    --> "npoints" should be taken for all geometries
- SQL Server 2008 gives "6"
    select geometry::STGeomFromText('MULTILINESTRING((1 1,2 2,3 3),(4 4,5 5,6 6))',0).STNumPoints()
- MySQL gives "NULL"

select 1 as code,'np p' as header,npoints(geomfromtext('POINT(1 2)')) as contents
union select 2,'length point', length(geomfromtext('POINT(1 2)'))
union select 3,'peri point', perimeter(geomfromtext('POINT(1 2)'))
union select 4,'area point',area(geomfromtext('POINT(1 2)'))


union select 5,'# ls',npoints(geomfromtext('LINESTRING(1 1,2 2,3 3)'))
union select 6,'length ls',length(geomfromtext('LINESTRING(1 1,2 2,3 3)'))
union select 7,'peri ls',perimeter(geomfromtext('LINESTRING(1 1,2 2,3 3)'))
union select 8,'aera ls',area(geomfromtext('LINESTRING(1 1,2 2,3 3)'))

union select 9,'# poly',npoints(geomfromtext('POLYGON((0 0,0 4,4 4,4 0,0 0),(1 1,1 2,2 2,2 1,1 1),(1 1,1 2,2 2,2 1,1 1))'))
union select 10,'length poly',length(geomfromtext('POLYGON((0 0,0 4,4 4,4 0,0 0),(1 1,1 2,2 2,2 1,1 1),(1 1,1 2,2 2,2 1,1 1))'))
union select 11,'peri poly',perimeter(geomfromtext('POLYGON((0 0,0 4,4 4,4 0,0 0),(1 1,1 2,2 2,2 1,1 1),(1 1,1 2,2 2,2 1,1 1))'))
union select 12,'area poly',area(geomfromtext('POLYGON((0 0,0 4,4 4,4 0,0 0),(1 1,1 2,2 2,2 1,1 1),(1 1,1 2,2 2,2 1,1 1))'))

*/
