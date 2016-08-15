// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014, 2015, 2016.
// Modifications copyright (c) 2014-2016 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_within.hpp"


#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

template <typename P>
void test_p_p()
{
    typedef bg::model::multi_point<P> mpt;

    test_geometry<P, P>("POINT(0 0)", "POINT(0 0)", true);
    test_geometry<P, P>("POINT(0 0)", "POINT(1 1)", false);

    test_geometry<P, mpt>("POINT(0 0)", "MULTIPOINT(0 0, 1 1)", true);
    test_geometry<P, mpt>("POINT(0 0)", "MULTIPOINT(1 1, 2 2)", false);
}

template <typename P>
void test_p_l()
{
    typedef bg::model::segment<P> seg;
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<P, seg>("POINT(1 1)", "LINESTRING(0 0, 2 2)", true);
    test_geometry<P, seg>("POINT(0 0)", "LINESTRING(0 0, 1 1)", false);
    test_geometry<P, seg>("POINT(1 0)", "LINESTRING(0 0, 1 1)", false);

    test_geometry<P, ls>("POINT(0 0)", "LINESTRING(0 0,1 1,2 2)", false);
    test_geometry<P, ls>("POINT(3 3)", "LINESTRING(0 0,1 1,2 2)", false);
    test_geometry<P, ls>("POINT(1 1)", "LINESTRING(0 0,2 2,3 3)", true);

    test_geometry<P, ls>("POINT(1 1)", "LINESTRING(0 0, 2 2)", true);
    test_geometry<P, ls>("POINT(0 0)", "LINESTRING(0 0, 1 1)", false);

    test_geometry<P, mls>("POINT(0 0)", "MULTILINESTRING((0 0,1 1,2 2),(0 0,0 1))", true);
    test_geometry<P, mls>("POINT(0 0)", "MULTILINESTRING((0 0,1 1,2 2),(0 0,0 1),(0 0,1 0))", false);
    
    test_geometry<P, mls>("POINT(1 1)", "MULTILINESTRING((0 0, 1 1),(1 1, 2 2))", true);
    test_geometry<P, mls>("POINT(1 1)", "MULTILINESTRING((0 0, 1 1),(2 2, 3 3))", false);
}

template <typename P>
void test_p_a()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    // trivial case
    test_ring<P>("POINT(1 1)", "POLYGON((0 0,0 2,2 2,2 0,0 0))", true, false);

    // on border/corner
    test_ring<P>("POINT(0 0)", "POLYGON((0 0,0 2,2 2,2 0,0 0))", false, true);
    test_ring<P>("POINT(0 1)", "POLYGON((0 0,0 2,2 2,2 0,0 0))", false, true);

    // aligned to segment/vertex
    test_ring<P>("POINT(1 1)", "POLYGON((0 0,0 3,3 3,3 1,2 1,2 0,0 0))", true, false);
    test_ring<P>("POINT(1 1)", "POLYGON((0 0,0 3,4 3,3 1,2 2,2 0,0 0))", true, false);

    // same polygon, but point on border
    test_ring<P>("POINT(3 3)", "POLYGON((0 0,0 3,3 3,3 1,2 1,2 0,0 0))", false, true);
    test_ring<P>("POINT(3 3)", "POLYGON((0 0,0 3,4 3,3 1,2 2,2 0,0 0))", false, true);

    // holes
    test_geometry<P, poly>("POINT(2 2)",
        "POLYGON((0 0,0 4,4 4,4 0,0 0),(1 1,3 1,3 3,1 3,1 1))", false);

    // Real-life problem (solved now), point is in the middle, 409623 is also a coordinate
    // on the border, has been wrong in the past (2009)
    test_ring<P>("POINT(146383 409623)",
        "POLYGON((146351 410597,146521 410659,147906 410363,148088 410420"
        ",148175 410296,148281 409750,148215 409623,148154 409666,148154 409666"
        ",148130 409625,148035 409626,148035 409626,148008 409544,147963 409510"
        ",147993 409457,147961 409352,147261 408687,147008 408586,145714 408840"
        ",145001 409033,144486 409066,144616 409308,145023 410286,145254 410488"
        ",145618 410612,145618 410612,146015 410565,146190 410545,146351 410597))",
        true, false);

    test_geometry<P, mpoly>("POINT(2 2)",
        "MULTIPOLYGON(((0 0,0 4,4 4,4 0,0 0),(1 1,3 1,3 3,1 3,1 1)),((5 5,5 9,9 9,9 5,5 5)))", false);
    test_geometry<P, mpoly>("POINT(1 1)",
        "MULTIPOLYGON(((0 0,0 4,4 4,4 0,0 0),(1 1,3 1,3 3,1 3,1 1)),((5 5,5 9,9 9,9 5,5 5)))", false);
    test_geometry<P, mpoly>("POINT(1 1)",
        "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(2 2,4 2,4 4,2 4,2 2)),((5 5,5 9,9 9,9 5,5 5)))", true);
    test_geometry<P, mpoly>("POINT(6 6)",
        "MULTIPOLYGON(((0 0,0 4,4 4,4 0,0 0),(1 1,3 1,3 3,1 3,1 1)),((5 5,5 9,9 9,9 5,5 5)))", true);

    test_geometry<P, poly>("POINT(6 4)",
                           "POLYGON((0 5, 5 0, 6 1, 5 2, 8 4, 5 6, 6 7, 5 8, 6 9, 5 10, 0 5))", true);
    test_geometry<P, poly>("POINT(4 6)",
                           "POLYGON((5 0, 0 5, 1 6, 2 5, 4 8, 6 5, 7 6, 8 5, 9 6, 10 5, 5 0))", true);
}

template <typename P>
void test_all()
{
    test_p_p<P>();
    test_p_l<P>();
    test_p_a<P>();
}

template <typename Point>
void test_spherical_geographic()
{
    bg::model::polygon<Point> wrangel;

    // SQL Server check (no geography::STWithin, so check with intersection trick)
    /*

    with q as (
    select geography::STGeomFromText('POLYGON((-178.569 71.5641,-179.034 71.5977,-179.305 71.5514,-179.629 71.5772,-180 71.5358,179.53 71.4383,178.872 71.2175,178.618 71.0355,178.791 70.7964,179.273 70.8886,179.678 70.8955,-180 70.9972,-179.274 70.9078,-178.819 70.98,-177.939 71.0375,-177.62 71.1166,-177.439 71.2269,-177.503 71.2775,-177.833 71.3461,-178.018 71.4497,-178.569 71.5641))',4326) as wrangel
    )

    select wrangel.STArea()/1000000.0
    ,geography::STGeomFromText('POINT(-179.3 71.27)',4326).STIntersection(wrangel).STAsText() as workaround_within_1
    ,geography::STGeomFromText('POINT(-179.9 70.95)',4326).STIntersection(wrangel).STAsText() as workaround_within_2
    ,geography::STGeomFromText('POINT(179.9 70.95)',4326).STIntersection(wrangel).STAsText() as workaround_within_3
    from q

    -> 7669.10402181435 POINT (-179.3 71.27) GEOMETRYCOLLECTION EMPTY GEOMETRYCOLLECTION EMPTY

    PostGIS knows Within for Geography neither, and the intersection trick gives the same result

    */

    bg::read_wkt("POLYGON((-178.568604 71.564148,-178.017548 71.449692,-177.833313 71.3461,-177.502838 71.277466 ,-177.439453 71.226929,-177.620026 71.116638,-177.9389 71.037491,-178.8186 70.979965,-179.274445 70.907761,-180 70.9972,179.678314 70.895538,179.272766 70.888596,178.791016 70.7964,178.617737 71.035538,178.872192 71.217484,179.530273 71.4383 ,-180 71.535843 ,-179.628601 71.577194,-179.305298 71.551361,-179.03421 71.597748,-178.568604 71.564148))", wrangel);

    bool within = bg::within(Point(-179.3, 71.27), wrangel);
    BOOST_CHECK_EQUAL(within, true);

    within = bg::within(Point(-179.9, 70.95), wrangel);
    BOOST_CHECK_EQUAL(within, false);

    within = bg::within(Point(179.9, 70.95), wrangel);
    BOOST_CHECK_EQUAL(within, false);

    // Test using great circle mapper
    // http://www.gcmap.com/mapui?P=5E52N-9E53N-7E50N-5E52N,7E52.5N,8E51.5N,6E51N

    bg::model::polygon<Point> triangle;
    bg::read_wkt("POLYGON((5 52,9 53,7 50,5 52))", triangle);
    BOOST_CHECK_EQUAL(bg::within(Point(7, 52.5), triangle), true);
    BOOST_CHECK_EQUAL(bg::within(Point(8.0, 51.5), triangle), false);
    BOOST_CHECK_EQUAL(bg::within(Point(6.0, 51.0), triangle), false);

    // northern hemisphere
    {
        bg::model::polygon<Point> poly_n;
        bg::read_wkt("POLYGON((10 50,30 50,30 40,10 40, 10 50))", poly_n);
        Point pt_n1(20, 50.00001);
        Point pt_n2(20, 40.00001);
        typedef bg::strategy::side::spherical_side_formula<> ssf;
        BOOST_CHECK_EQUAL(ssf::apply(poly_n.outer()[0], poly_n.outer()[1], pt_n1), -1); // right of segment
        BOOST_CHECK_EQUAL(ssf::apply(poly_n.outer()[2], poly_n.outer()[3], pt_n2), 1); // left of segment
        BOOST_CHECK_EQUAL(bg::within(pt_n1, poly_n), true);
        BOOST_CHECK_EQUAL(bg::within(pt_n2, poly_n), false);
    }
    // southern hemisphere
    {
        bg::model::polygon<Point> poly_s;
        bg::read_wkt("POLYGON((10 -40,30 -40,30 -50,10 -50, 10 -40))", poly_s);
        Point pt_s1(20, -40.00001);
        Point pt_s2(20, -50.00001);
        typedef bg::strategy::side::spherical_side_formula<> ssf;
        BOOST_CHECK_EQUAL(ssf::apply(poly_s.outer()[0], poly_s.outer()[1], pt_s1), 1); // left of segment
        BOOST_CHECK_EQUAL(ssf::apply(poly_s.outer()[2], poly_s.outer()[3], pt_s2), -1); // right of segment
        BOOST_CHECK_EQUAL(bg::within(pt_s1, poly_s), false);
        BOOST_CHECK_EQUAL(bg::within(pt_s2, poly_s), true);
    }
    // crossing antimeridian, northern hemisphere
    {
        bg::model::polygon<Point> poly_n;
        bg::read_wkt("POLYGON((170 50,-170 50,-170 40,170 40, 170 50))", poly_n);
        Point pt_n11(180, 50.00001);
        Point pt_n12(-180, 50.00001);
        Point pt_n13(179, 50.00001);
        Point pt_n14(-179, 50.00001);
        Point pt_n21(180, 40.00001);
        Point pt_n22(-180, 40.00001);
        Point pt_n23(179, 40.00001);
        Point pt_n24(-179, 40.00001);
        BOOST_CHECK_EQUAL(bg::within(pt_n11, poly_n), true);
        BOOST_CHECK_EQUAL(bg::within(pt_n12, poly_n), true);
        BOOST_CHECK_EQUAL(bg::within(pt_n13, poly_n), true);
        BOOST_CHECK_EQUAL(bg::within(pt_n14, poly_n), true);
        BOOST_CHECK_EQUAL(bg::within(pt_n21, poly_n), false);
        BOOST_CHECK_EQUAL(bg::within(pt_n22, poly_n), false);
        BOOST_CHECK_EQUAL(bg::within(pt_n23, poly_n), false);
        BOOST_CHECK_EQUAL(bg::within(pt_n24, poly_n), false);
    }

    // TODO: Move to covered_by tests

    // Segment going through pole
    {
        bg::model::polygon<Point> poly_n1;
        bg::read_wkt("POLYGON((-90 80,90 80,90 70,-90 70, -90 80))", poly_n1);
        // Points on segment
        BOOST_CHECK_EQUAL(bg::covered_by(Point(-90, 85), poly_n1), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(90, 85), poly_n1), true);
        // Points on pole
        BOOST_CHECK_EQUAL(bg::covered_by(Point(90, 90), poly_n1), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(0, 90), poly_n1), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(45, 90), poly_n1), true);
    }
    // Segment going through pole
    {
        bg::model::polygon<Point> poly_n2;
        bg::read_wkt("POLYGON((-90 80,90 70,0 70,-90 80))", poly_n2);
        // Points on segment
        BOOST_CHECK_EQUAL(bg::covered_by(Point(-90, 85), poly_n2), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(90, 75), poly_n2), true);
        // Points outside but on the same level as segment
        BOOST_CHECK_EQUAL(bg::covered_by(Point(-90, 75), poly_n2), false);
    }
    // Possibly invalid, 2-segment polygon with segment going through pole
    /*{
        bg::model::polygon<Point> poly_n;
        bg::read_wkt("POLYGON((-90 80,90 70,-90 80))", poly_n);
        // Point within
        BOOST_CHECK_EQUAL(bg::within(Point(0, 89), poly_n), true);
        // Points on segment
        BOOST_CHECK_EQUAL(bg::covered_by(Point(-90, 85), poly_n), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(90, 75), poly_n), true);
        // Points outside but on the same level as segment
        BOOST_CHECK_EQUAL(bg::covered_by(Point(-90, 75), poly_n), false);
    }*/
    // Segment endpoints on pole with arbitrary longitudes
    {
        bg::model::polygon<Point> poly_n3;
        bg::read_wkt("POLYGON((45 90,45 80,0 80,45 90))", poly_n3);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(0, 85), poly_n3), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(45, 85), poly_n3), true);
    }
    // Segment going through pole
    {
        bg::model::polygon<Point> poly_s1;
        bg::read_wkt("POLYGON((-90 -80,-90 -70,90 -70,90 -80,-90 -80))", poly_s1);
        // Points on segment
        BOOST_CHECK_EQUAL(bg::covered_by(Point(-90, -85), poly_s1), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(90, -85), poly_s1), true);
        // Points on pole
        BOOST_CHECK_EQUAL(bg::covered_by(Point(90, -90), poly_s1), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(0, -90), poly_s1), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(45, -90), poly_s1), true);
    }
    // Segment endpoints on pole with arbitrary longitudes
    {
        bg::model::polygon<Point> poly_s2;
        bg::read_wkt("POLYGON((45 -90,0 -80,45 -80,45 -90))", poly_s2);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(0, -85), poly_s2), true);
        BOOST_CHECK_EQUAL(bg::covered_by(Point(45, -85), poly_s2), true);
    }
}

void test_large_integers()
{
    typedef bg::model::point<int, 2, bg::cs::cartesian> int_point_type;
    typedef bg::model::point<double, 2, bg::cs::cartesian> double_point_type;

    std::string const polygon_li = "POLYGON((1872000 528000,1872000 192000,1536119 192000,1536000 528000,1200000 528000,1200000 863880,1536000 863880,1872000 863880,1872000 528000))";
    bg::model::polygon<int_point_type> int_poly;
    bg::model::polygon<double_point_type> double_poly;
    bg::read_wkt(polygon_li, int_poly);
    bg::read_wkt(polygon_li, double_poly);

    std::string const point_li = "POINT(1592000 583950)";
    int_point_type int_point;
    double_point_type double_point;
    bg::read_wkt(point_li, int_point);
    bg::read_wkt(point_li, double_point);

    bool wi = bg::within(int_point, int_poly);
    bool wd = bg::within(double_point, double_poly);

    BOOST_CHECK_MESSAGE(wi == wd, "within<a double> different from within<an int>");
}

void test_tickets()
{
    typedef boost::geometry::model::d2::point_xy<double> pt;
    typedef boost::geometry::model::ring<pt> ring;

    // https://svn.boost.org/trac/boost/ticket/9628
    {
        ring r;
        r.push_back(pt(-19155.669324773193,54820.312032458620));
        r.push_back(pt(-13826.169324773080,54820.312032458627));
        r.push_back(pt(-13826.169324773078,52720.312032458663));
        r.push_back(pt(-12755.169324773129,52720.312032458663));
        r.push_back(pt(-12755.169324773129,51087.312032458671));
        r.push_back(pt(-12760.669324773080,51087.312032458671));
        r.push_back(pt(-12760.669324773082,51070.312032458627));
        r.push_back(pt(-19155.669324779392,51070.312032458620));
        r.push_back(pt(-19155.669324773193,54820.312032458620));

        pt p( -12260.669324773118, 54820.312032458634 );

        //boost::geometry::correct(r);

        bool within = boost::geometry::within(p, r);
        BOOST_CHECK_EQUAL(within, false);
    }
    // similar
    {
        ring r;
        r.push_back(pt(-14155.6,54820.312032458620));
        r.push_back(pt(-13826.1,54820.312032458625));
        r.push_back(pt(-12155.6,53720.3));
        r.push_back(pt(-14155.6,54820.312032458620));

        pt p( -13826.0, 54820.312032458634 );

        bool within = boost::geometry::within(p, r);
        BOOST_CHECK_EQUAL(within, false);
    }

    // https://svn.boost.org/trac/boost/ticket/10234
    {
        pt p;
        ring r;
        bg::read_wkt("POINT(0.1377 5.00)", p);
        bg::read_wkt("POLYGON((0.1277 4.97,  0.1277 5.00, 0.1278 4.9999999999999982, 0.1278 4.97, 0.1277 4.97))", r);
        bool within = boost::geometry::within(p, r);
        BOOST_CHECK_EQUAL(within, false);
        bool covered_by = boost::geometry::covered_by(p, r);
        BOOST_CHECK_EQUAL(covered_by, false);
    }
}

int test_main( int , char* [] )
{
    test_large_integers();

    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

    test_spherical_geographic<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_spherical_geographic<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
    test_spherical_geographic<bg::model::point<ttmath_big, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_spherical_geographic<bg::model::point<ttmath_big, 2, bg::cs::geographic<bg::degree> > >();
#endif

    test_tickets();

    return 0;
}
