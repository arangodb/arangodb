// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2013-2020.
// Modifications copyright (c) 2013-2020 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_equals.hpp"

#include <boost/type_traits/is_floating_point.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>


namespace bgm = bg::model;

template <typename P>
void test_pointlike()
{
    typedef bgm::multi_point<P> mpt;

    test_geometry<P, P>("ptpt2d_1", "POINT(0 0)", "POINT(0 0)", true);
    test_geometry<P, P>("ptpt2d_2", "POINT(0 0)", "POINT(1 1)", false);

    test_geometry<P, mpt>("ptmpt2d_1", "POINT(0 0)", "MULTIPOINT(0 0)", true);
    test_geometry<P, mpt>("ptmpt2d_1", "POINT(0 0)", "MULTIPOINT(0 0, 1 1)", false);

    test_geometry<mpt, P>("mptpt2d_1", "MULTIPOINT(0 0)", "POINT(0 0)", true);
    test_geometry<mpt, P>("mptpt2d_1", "MULTIPOINT(0 0, 1 1)", "POINT(0 0)", false);

    test_geometry<mpt, mpt>("mptmpt2d_1", "MULTIPOINT(0 0, 1 1)", "MULTIPOINT(0 0, 1 1)", true);
    test_geometry<mpt, mpt>("mptmpt2d_1", "MULTIPOINT(0 0, 1 1)", "MULTIPOINT(0 0, 2 2)", false);
    test_geometry<mpt, mpt>("mptmpt2d_1", "MULTIPOINT(0 0, 1 1)", "MULTIPOINT(2 2, 3 3)", false);
}

template <typename P>
void test_segment_segment()
{
    typedef bgm::segment<P> seg;

    test_geometry<seg, seg>("seg2d_1", "LINESTRING(0 0, 3 3)", "LINESTRING(0 0, 3 3)", true);
    test_geometry<seg, seg>("seg2d_1", "LINESTRING(0 0, 3 3)", "LINESTRING(3 3, 0 0)", true);

    test_geometry<seg, seg>("seg2d_1", "LINESTRING(0 0, 3 3)", "LINESTRING(0 0, 1 1)", false);
    test_geometry<seg, seg>("seg2d_1", "LINESTRING(0 0, 3 3)", "LINESTRING(3 3, 2 2)", false);

    test_geometry<seg, seg>("seg2d_1", "LINESTRING(0 0, 3 3)", "LINESTRING(1 1, 4 4)", false);
    test_geometry<seg, seg>("seg2d_1", "LINESTRING(0 0, 3 3)", "LINESTRING(1 0, 2 0)", false);
}

template <typename P>
void test_linestring_linestring()
{
    typedef bgm::linestring<P> ls;

    test_geometry<ls, ls>("ls2d_1", "LINESTRING(1 1, 3 3)", "LINESTRING(3 3, 1 1)", true);
    test_geometry<ls, ls>("ls2d_2", "LINESTRING(1 1, 3 3, 2 5)", "LINESTRING(1 1, 2 2, 3 3, 2 5)", true);
    test_geometry<ls, ls>("ls2d_3", "LINESTRING(1 0, 3 3, 2 5)", "LINESTRING(1 1, 2 2, 3 3, 2 5)", false);
    test_geometry<ls, ls>("ls2d_4", "LINESTRING(1 0, 3 3, 2 5)", "LINESTRING(1 1, 3 3, 2 5)", false);
    test_geometry<ls, ls>("ls2d_5", "LINESTRING(0 5,5 5,10 5,10 0,5 0,5 5,5 10,10 10,15 10,15 5,10 5,10 10,10 15)",
                                    "LINESTRING(0 5,15 5,15 10,5 10,5 0,10 0,10 15)", true);
    test_geometry<ls, ls>("ls2d_6", "LINESTRING(0 5,5 5,10 5,10 10,5 10,5 5,5 0)", "LINESTRING(0 5,5 5,5 10,10 10,10 5,5 5,5 0)", true);
    test_geometry<ls, ls>("ls2d_7", "LINESTRING(0 5,10 5,10 10,5 10,5 0)", "LINESTRING(0 5,5 5,5 10,10 10,10 5,5 5,5 0)", true);
    test_geometry<ls, ls>("ls2d_8", "LINESTRING(0 0,5 0,5 0,6 0)", "LINESTRING(0 0,6 0)", true);

    test_geometry<ls, ls>("ls2d_seg", "LINESTRING(1 1,2 2)", "LINESTRING(1 1,2 2)", true);
    test_geometry<ls, ls>("ls2d_rev", "LINESTRING(1 1,2 2)", "LINESTRING(2 2,1 1)", true);

    test_geometry<ls, ls>("ls2d_spike", "LINESTRING(0 0,5 0,3 0,6 0)", "LINESTRING(0 0,6 0)", true);

    test_geometry<ls, ls>("ls2d_ring1", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "LINESTRING(5 5,0 5,0 0,5 0,5 5)", true);
    test_geometry<ls, ls>("ls2d_ring2", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "LINESTRING(5 5,5 0,0 0,0 5,5 5)", true);
    test_geometry<ls, ls>("ls2d_overl_ring1", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "LINESTRING(5 5,0 5,0 0,5 0,5 5,0 5)", true);
    test_geometry<ls, ls>("ls2d_overl_ring2", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "LINESTRING(5 5,5 0,0 0,0 5,5 5,5 0)", true);

    // https://svn.boost.org/trac/boost/ticket/10904
    if ( BOOST_GEOMETRY_CONDITION(
            boost::is_floating_point<typename bg::coordinate_type<ls>::type>::value ) )
    {
        test_geometry<ls, ls>("ls2d_small1",
                              "LINESTRING(5.6956521739130430148634331999347 -0.60869565217391330413931882503675,5.5 -0.50000000000000066613381477509392)",
                              "LINESTRING(5.5 -0.50000000000000066613381477509392,5.5 -0.5)",
                              false);

        test_geometry<ls, ls>("ls2d_small2",
                              "LINESTRING(-3.2333333333333333925452279800083 5.5999999999999978683717927196994,-3.2333333333333333925452279800083 5.5999999999999996447286321199499)",
                              "LINESTRING(-3.2333333333333325043668082798831 5.5999999999999996447286321199499,-3.2333333333333333925452279800083 5.5999999999999996447286321199499)",
                              false);
    }
}

template <typename P>
void test_linestring_multilinestring()
{
    typedef bgm::linestring<P> ls;
    typedef bgm::multi_linestring<ls> mls;

    test_geometry<ls, mls>("ls_mls_1", "LINESTRING(0 0,1 0,2 0)", "MULTILINESTRING((0 0,2 0))", true);
    test_geometry<ls, mls>("ls_mls_1", "LINESTRING(0 0,1 0,2 0)", "MULTILINESTRING((0 0,1 0),(1 0,2 0))", true);
    test_geometry<ls, mls>("ls_mls_1", "LINESTRING(0 0,2 0,4 0)", "MULTILINESTRING((0 0,2 0),(2 0,3 0),(3 0,4 0))", true);
    test_geometry<ls, mls>("ls_mls_1", "LINESTRING(0 0,2 0,4 0)", "MULTILINESTRING((0 0,2 0),(2 0,3 0),(2 0,3 0),(3 0,4 0))", true);
    test_geometry<ls, mls>("ls_mls_1", "LINESTRING(0 0,2 0,4 0)", "MULTILINESTRING((0 0,2 0),(3 0,4 0))", false);

    test_geometry<ls, mls>("ls_mls_spike", "LINESTRING(0 0,2 0,2 2,2 0,4 0)", "MULTILINESTRING((0 0,4 0),(2 2,2 0))", true);
    test_geometry<ls, mls>("ls_mls_spike", "LINESTRING(0 0,2 0,2 2,2 0,4 0)", "MULTILINESTRING((0 0,4 0),(2 2,2 -1))", false);

    test_geometry<ls, mls>("ls_mls_ring1", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "MULTILINESTRING((5 5,0 5,0 0),(0 0,5 0,5 5))", true);
    test_geometry<ls, mls>("ls_mls_ring2", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "MULTILINESTRING((5 5,5 0,0 0),(0 0,0 5,5 5))", true);
    test_geometry<ls, mls>("ls_mls_overl_ring1", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "MULTILINESTRING((5 5,0 5,0 0),(0 0,5 0,5 5,0 5))", true);
    test_geometry<ls, mls>("ls_mls_overl_ring2", "LINESTRING(0 0,5 0,5 5,0 5,0 0)", "MULTILINESTRING((5 5,5 0,0 0),(0 0,0 5,5 5,5 0))", true);
}

template <typename P>
void test_multilinestring_multilinestring()
{
    typedef bgm::linestring<P> ls;
    typedef bgm::multi_linestring<ls> mls;

    test_geometry<mls, mls>("ls_mls_mls",
                            "MULTILINESTRING((0 5,10 5,10 10,5 10),(5 10,5 0,5 2),(5 2,5 5,0 5))",
                            "MULTILINESTRING((5 5,0 5),(5 5,5 0),(10 10,10 5,5 5,5 10,10 10))",
                            true);
}

template <typename P>
void test_polygons()
{
    typedef bg::model::polygon<P, true, true> poly_cw_c;
    typedef bg::model::polygon<P, true, false> poly_cw_o;
    typedef bg::model::polygon<P, false, true> poly_ccw_c;
    typedef bg::model::polygon<P, false, false> poly_ccw_o;
    typedef bg::model::box<P> box;

    std::string wkt1 = "POLYGON((-18 1, -23 1, -23 -3, -18 -3))";
    std::string wkt2 = "POLYGON((-23 1, -23 -3, -18 -3, -18 1))";

    test_geometry<poly_cw_c, poly_cw_c>("polys_cw_c_cw_c", wkt1, wkt2, true, true);
    test_geometry<poly_cw_c, poly_cw_o>("polys_cw_c_cw_o", wkt1, wkt2, true, true);
    test_geometry<poly_cw_c, poly_ccw_c>("polys_cw_c_ccw_c", wkt1, wkt2, true, true);
    test_geometry<poly_cw_c, poly_ccw_o>("polys_cw_c_ccw_o", wkt1, wkt2, true, true);
    test_geometry<poly_cw_c, box>("polys_cw_c_box", wkt1, wkt2, true, true);

    test_geometry<poly_cw_o, poly_cw_c>("polys_cw_o_cw_c", wkt1, wkt2, true, true);
    test_geometry<poly_cw_o, poly_cw_o>("polys_cw_o_cw_o", wkt1, wkt2, true, true);
    test_geometry<poly_cw_o, poly_ccw_c>("polys_cw_o_ccw_c", wkt1, wkt2, true, true);
    test_geometry<poly_cw_o, poly_ccw_o>("polys_cw_o_ccw_o", wkt1, wkt2, true, true);
    test_geometry<poly_cw_o, box>("polys_cw_o_box", wkt1, wkt2, true, true);

    test_geometry<poly_ccw_c, poly_cw_c>("polys_ccw_c_cw_c", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_c, poly_cw_o>("polys_ccw_c_cw_o", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_c, poly_ccw_c>("polys_ccw_c_ccw_c", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_c, poly_ccw_o>("polys_ccw_c_ccw_o", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_c, box>("polys_cw_o_box", wkt1, wkt2, true, true);

    test_geometry<poly_ccw_o, poly_cw_c>("polys_ccw_o_cw_c", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_o, poly_cw_o>("polys_ccw_o_cw_o", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_o, poly_ccw_c>("polys_ccw_o_ccw_c", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_o, poly_ccw_o>("polys_ccw_o_ccw_o", wkt1, wkt2, true, true);
    test_geometry<poly_ccw_o, box>("polys_ccw_o_box", wkt1, wkt2, true, true);
}

template <typename P>
void test_all()
{
    typedef bg::model::box<P> box;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> polygon;
    //typedef bg::model::linestring<P> linestring;

    std::string case_p1 = "POLYGON((0 0,0 2,2 2,0 0))";

    test_geometry<P, P>("p1", "POINT(1 1)", "POINT(1 1)", true);
    test_geometry<P, P>("p2", "POINT(1 1)", "POINT(1 2)", false);
    test_geometry<box, box>("b1", "BOX(1 1,2 2)", "BOX(1 2,2 2)", false);
    test_geometry<box, box>("b1", "BOX(1 2,3 4)", "BOX(1 2,3 4)", true);

    // Completely equal
    test_geometry<ring, ring>("poly_eq", case_p1, case_p1, true);

    // Shifted
    test_geometry<ring, ring>("poly_sh", "POLYGON((2 2,0 0,0 2,2 2))", case_p1, true);
    test_geometry<polygon, polygon>("poly_sh2", case_p1, "POLYGON((0 2,2 2,0 0,0 2))", true);

    // Extra coordinate
    test_geometry<ring, ring>("poly_extra", case_p1, "POLYGON((0 0,0 2,2 2,1 1,0 0))", true);

    // Shifted + extra (redundant) coordinate
    test_geometry<ring, ring>("poly_shifted_extra1", "POLYGON((2 2,1 1,0 0,0 2,2 2))", case_p1, true);

    // Shifted + extra (redundant) coordinate being first/last point
    test_geometry<ring, ring>("poly_shifted_extra2", "POLYGON((1 1,0 0,0 2,2 2,1 1))", case_p1, true);

    // Degenerate (duplicate) points
    test_geometry<ring, ring>("poly_degenerate", "POLYGON((0 0,0 2,2 2,2 2,0 0))", "POLYGON((0 0,0 2,0 2,2 2,0 0))", true);

    // Two different bends, same area, unequal
    test_geometry<ring, ring>("poly_bends",
        "POLYGON((4 0,5 3,8 4,7 7,4 8,0 4,4 0))",
        "POLYGON((4 0,7 1,8 4,5 5,4 8,0 4,4 0))", false);

    // Unequal (but same area)
    test_geometry<ring, ring>("poly_uneq", case_p1, "POLYGON((1 1,1 3,3 3,1 1))", false);

    // Note that POLYGON((0 0,0 4,4 4,0 0),(1 1,2 1,2 2,1 2,1 1))
    // below is invalid. equals() returns different result than
    // relate() with equals mask in this case because for areal
    // geometries different algorithms is used, i.e. collect_vectors.

    // One having hole
    test_geometry<polygon, polygon>("poly_hole",
            "POLYGON((0 0,0 4,4 4,0 0))",
            "POLYGON((0 0,0 4,4 4,0 0),(1 1,2 1,2 2,1 2,1 1))", false);

    // Both having holes
    test_geometry<polygon, polygon>("poly_holes",
            "POLYGON((0 0,0 4,4 4,0 0),(1 1,2 1,2 2,1 2,1 1))",
            "POLYGON((0 0,0 4,4 4,0 0),(1 1,2 1,2 2,1 2,1 1))", true);

    // Both having holes, outer equal, inner not equal
    test_geometry<polygon, polygon>("poly_uneq_holes",
            "POLYGON((0 0,0 4,4 4,0 0),(1 1,2 1,2 2,1 2,1 1))",
            "POLYGON((0 0,0 4,4 4,0 0),(2 2,3 2,3 3,2 3,2 2))", false);

    // Both having 2 holes, equal but in different order
    test_geometry<polygon, polygon>("poly_holes_diff_order",
            "POLYGON((0 0,0 4,4 4,0 0),(1 1,2 1,2 2,1 2,1 1),(2 2,3 2,3 3,2 3,2 2))",
            "POLYGON((0 0,0 4,4 4,0 0),(2 2,3 2,3 3,2 3,2 2),(1 1,2 1,2 2,1 2,1 1))", true);

    // Both having 3 holes, equal but in different order
    test_geometry<polygon, polygon>("poly_holes_diff_order_3",
            "POLYGON((0 0,0 10,10 10,0 0),(1 1,2 1,2 2,1 2,1 1),(4 1,5 1,5 2,4 2,4 1),(2 2,3 2,3 3,2 3,2 2))",
            "POLYGON((0 0,0 10,10 10,0 0),(4 1,5 1,5 2,4 2,4 1),(2 2,3 2,3 3,2 3,2 2),(1 1,2 1,2 2,1 2,1 1))", true);

    // polygon/ring vv
    test_geometry<polygon, ring>("poly_sh2_pr", case_p1, case_p1, true);
    test_geometry<ring, polygon>("poly_sh2_rp", case_p1, case_p1, true);

    // box/ring/poly
    test_geometry<box, ring>("boxring1", "BOX(1 1,2 2)", "POLYGON((1 1,1 2,2 2,2 1,1 1))", true);
    test_geometry<ring, box>("boxring2", "POLYGON((1 1,1 2,2 2,2 1,1 1))", "BOX(1 1,2 2)", true);
    test_geometry<box, polygon>("boxpoly1", "BOX(1 1,2 2)", "POLYGON((1 1,1 2,2 2,2 1,1 1))", true);
    test_geometry<polygon, box>("boxpoly2", "POLYGON((1 1,1 2,2 2,2 1,1 1))", "BOX(1 1,2 2)", true);

    test_geometry<polygon, box>("boxpoly2", "POLYGON((1 1,1 2,2 2,2 1,1 1))", "BOX(1 1,2 3)", false);

    test_geometry<polygon, polygon>("poly_holes_shifted_points",
        "POLYGON((0 0,0 3,3 3,3 0,0 0),(1 1,2 1,2 2,1 2,1 1))",
        "POLYGON((0 0,0 3,3 3,3 0,0 0),(2 2,1 2,1 1,2 1,2 2))", true);

    test_pointlike<P>();
    test_segment_segment<P>();
    test_linestring_linestring<P>();
    test_linestring_multilinestring<P>();
    test_multilinestring_multilinestring<P>();
    test_polygons<P>();
}


template <typename T>
void verify()
{
    T dxn1, dyn1, dxn2, dyn2;

    {
        T x1 = "0", y1 = "0", x2 = "3", y2 = "3";
        T dx = x2 - x1, dy = y2 - y1;
        T mag = sqrt(dx * dx + dy * dy);
        dxn1 = dx / mag;
        dyn1 = dy / mag;
    }

    {
        T x1 = "0", y1 = "0", x2 = "1", y2 = "1";
        T dx = x2 - x1, dy = y2 - y1;
        T mag = sqrt(dx * dx + dy * dy);
        dxn2 = dx / mag;
        dyn2 = dy / mag;
    }

    if (dxn1 == dxn2 && dyn1 == dyn2)
    {
        //std::cout << "vectors are equal, using ==" << std::endl;
    }
    if (boost::geometry::math::equals(dxn1, dxn2)
        && boost::geometry::math::equals(dyn1, dyn2))
    {
        //std::cout << "vectors are equal, using bg::math::equals" << std::endl;
    }

    bool equals = boost::geometry::math::equals_with_epsilon(dxn1, dxn2)
        && boost::geometry::math::equals_with_epsilon(dyn1, dyn2);

    if (equals)
    {
        //std::cout << "vectors are equal, using bg::math::equals_with_epsilon" << std::endl;
    }

    BOOST_CHECK_EQUAL(equals, true);
}


int test_main( int , char* [] )
{
    //verify<double>();

    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
