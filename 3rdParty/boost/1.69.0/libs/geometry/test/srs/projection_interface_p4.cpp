// Boost.Geometry
// Unit Test

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/srs/projection.hpp>

#include "check_geometry.hpp"

int test_main(int, char*[])
{
    using namespace boost::geometry;
    using namespace boost::geometry::model;
    using namespace boost::geometry::srs;

    typedef point<double, 2, cs::geographic<degree> > point_ll;
    typedef point<double, 2, cs::cartesian> point_xy;

    {
        point_ll pt_ll(1, 1);
        point_ll pt_ll2(0, 0);
        point_xy pt_xy(0, 0);

        projection<> prj = proj4("+proj=tmerc +ellps=WGS84 +units=m");
        
        prj.forward(pt_ll, pt_xy);
        test::check_geometry(pt_xy, "POINT(111308.33561309829 110591.34223734379)", 0.001);

        prj.inverse(pt_xy, pt_ll2);
        test::check_geometry(pt_ll2, "POINT(1 1)", 0.001);
    }

    // run-time errors
    {
        point_ll pt_ll(1, 1);
        point_xy pt_xy(0, 0);

        BOOST_CHECK_THROW(projection<> prj1((proj4(""))), bg::projection_exception);
        
        BOOST_CHECK_THROW(projection<> prj1((proj4("+proj=abcd"))), bg::projection_exception);
        
        projection<> prj3 = proj4("+proj=bacon +a=6400000");
        BOOST_CHECK_THROW(prj3.inverse(pt_xy, pt_ll), bg::projection_exception);
    }

    {
        segment<point_ll> seg_ll;
        segment<point_xy> seg_xy;
        linestring<point_ll> ls_ll;
        linestring<point_xy> ls_xy;
        ring<point_ll> ring_ll;
        ring<point_xy> ring_xy;
        polygon<point_ll> poly_ll;
        polygon<point_xy> poly_xy;
        multi_point<point_ll> mpt_ll;
        multi_point<point_xy> mpt_xy;
        multi_linestring<linestring<point_ll> > mls_ll;
        multi_linestring<linestring<point_xy> > mls_xy;
        multi_polygon<polygon<point_ll> > mpoly_ll;
        multi_polygon<polygon<point_xy> > mpoly_xy;

        bg::read_wkt("LINESTRING(0 0, 1 1)", seg_ll);
        bg::read_wkt("LINESTRING(0 0, 1 1, 2 2)", ls_ll);
        bg::read_wkt("POLYGON((0 0, 0 1, 1 1, 1 0, 0 0))", ring_ll);
        bg::read_wkt("POLYGON((0 0, 0 3, 3 3, 3 0, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1))", poly_ll);
        bg::read_wkt("MULTIPOINT(0 0, 1 1, 2 2)", mpt_ll);
        bg::read_wkt("MULTILINESTRING((0 0, 1 1),(2 2, 3 3))", mls_ll);
        bg::read_wkt("MULTIPOLYGON(((0 0, 0 3, 3 3, 3 0, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1)),((3 3,3 4,4 4,4 3,3 3)))", mpoly_ll);

        projection<> prj = proj4("+proj=tmerc +ellps=WGS84 +units=m");

        prj.forward(seg_ll, seg_xy);
        prj.forward(ls_ll, ls_xy);
        prj.forward(ring_ll, ring_xy);
        prj.forward(poly_ll, poly_xy);
        prj.forward(mpt_ll, mpt_xy);
        prj.forward(mls_ll, mls_xy);
        prj.forward(mpoly_ll, mpoly_xy);

        test::check_geometry(seg_xy, "LINESTRING(0 0,111308 110591)", 0.001);
        test::check_geometry(ls_xy, "LINESTRING(0 0,111308 110591,222550 221285)", 0.001);
        test::check_geometry(ring_xy, "POLYGON((0 0,0 110574,111308 110591,111325 0,0 0))", 0.001);
        test::check_geometry(poly_xy, "POLYGON((0 0,0 331726,333657 332183,334112 0,0 0),(111308 110591,222651 110642,222550 221285,111258 221183,111308 110591))", 0.001);
        test::check_geometry(mpt_xy, "MULTIPOINT((0 0),(111308 110591),(222550 221285))", 0.001);
        test::check_geometry(mls_xy, "MULTILINESTRING((0 0,111308 110591),(222550 221285,333657 332183))", 0.001);
        test::check_geometry(mpoly_xy, "MULTIPOLYGON(((0 0,0 331726,333657 332183,334112 0,0 0),(111308 110591,222651 110642,222550 221285,111258 221183,111308 110591)),((333657 332183,333302 442913,444561 443388,445034 332540,333657 332183)))", 0.001);
                
        bg::clear(seg_ll);
        bg::clear(ls_ll);
        bg::clear(ring_ll);
        bg::clear(poly_ll);
        bg::clear(mpt_ll);
        bg::clear(mls_ll);
        bg::clear(mpoly_ll);

        prj.inverse(seg_xy, seg_ll);
        prj.inverse(ls_xy, ls_ll);
        prj.inverse(ring_xy, ring_ll);
        prj.inverse(poly_xy, poly_ll);
        prj.inverse(mpt_xy, mpt_ll);
        prj.inverse(mls_xy, mls_ll);
        prj.inverse(mpoly_xy, mpoly_ll);

        test::check_geometry(seg_ll, "LINESTRING(0 0, 1 1)", 0.001);
        test::check_geometry(ls_ll, "LINESTRING(0 0, 1 1, 2 2)", 0.001);
        test::check_geometry(ring_ll, "POLYGON((0 0, 0 1, 1 1, 1 0, 0 0))", 0.001);
        test::check_geometry(poly_ll, "POLYGON((0 0, 0 3, 3 3, 3 0, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1))", 0.001);
        test::check_geometry(mpt_ll, "MULTIPOINT(0 0, 1 1, 2 2)", 0.001);
        test::check_geometry(mls_ll, "MULTILINESTRING((0 0, 1 1),(2 2, 3 3))", 0.001);
        test::check_geometry(mpoly_ll, "MULTIPOLYGON(((0 0, 0 3, 3 3, 3 0, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1)),((3 3,3 4,4 4,4 3,3 3)))", 0.001);
    }

    return 0;
}
