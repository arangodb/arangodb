// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014-2021.
// Modifications copyright (c) 2014-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Instead of having a separate (and nearly similar) unit test to test multipolygons,
// we now include them here and compile them by default. Only undefining the next line
// will avoid testing multi-geometries
#define BOOST_GEOMETRY_UNIT_TEST_MULTI

#include <geometry_test_common.hpp>

// The include to test
#include <boost/geometry/algorithms/point_on_surface.hpp>

// Helper includes
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

// Include from unit tests
#include <algorithms/test_overlay.hpp>
#include <algorithms/predef_relop.hpp>
#include <algorithms/overlay/overlay_cases.hpp>

#if defined(BOOST_GEOMETRY_UNIT_TEST_MULTI)
#  include <boost/geometry/algorithms/detail/extreme_points.hpp>
#endif


#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif

template <typename Geometry>
void test_geometry(std::string const& case_id, Geometry const& geometry, double /*expected_x*/ = 0, double /*expected_y*/ = 0)
{
//std::cout << case_id << std::endl;
    typedef typename bg::point_type<Geometry>::type point_type;

    point_type point;
    bg::point_on_surface(geometry, point);

    BOOST_CHECK_MESSAGE(bg::within(point, geometry),
        case_id << " generated point_on_surface (dim 1) is not within geometry");

#ifdef BOOST_GEOMETRY_POINT_ON_SURFACE_COMPLETE
    // For the algorithm we also check generation in the other dimension
    point_type right_point;
    bg::detail::point_on_surface::calculate_point_on_surface<0>(geometry, right_point);

    BOOST_CHECK_MESSAGE(bg::within(right_point, geometry),
        case_id << " generated point_on_surface (dim 0) is not within geometry");

    point_type returned_point = bg::return_point_on_surface(geometry);
#endif

#if defined(TEST_WITH_SVG)
    {
        std::ostringstream filename;
        filename << "point_on_surface_" << case_id << "_"
            << string_from_type<typename bg::coordinate_type<Geometry>::type>::name()
            << ".svg";

        std::ofstream svg(filename.str().c_str());

        // To map the intermediate products:
        bg::model::linestring<point_type> top_points;
        typedef bg::model::linestring<point_type> intruder_type;
        std::vector<intruder_type> top_intruders;
        bg::extreme_points<1>(geometry, top_points, top_intruders);

#ifdef BOOST_GEOMETRY_POINT_ON_SURFACE_COMPLETE
        bg::model::linestring<point_type> right_points;
        std::vector<bg::model::linestring<point_type> > right_intruders;
        bg::extreme_points<0>(geometry, right_points, right_intruders);
#endif

        bg::svg_mapper<point_type> mapper(svg, 500, 500);
        mapper.add(geometry);
        mapper.map(geometry, "fill-opacity:0.5;fill:rgb(153,204,0);stroke:rgb(153,204,0);stroke-width:1");

        // Top (red/magenta)
        mapper.map(top_points, "stroke:rgb(255,0,0);stroke-width:2");
        for (intruder_type const& intruder : top_intruders)
        {
            mapper.map(intruder, "stroke:rgb(255,0,255);stroke-width:2");
        }
        mapper.map(point, "opacity:0.8;fill:rgb(255,128,0);stroke:rgb(0,0,100);stroke-width:1", 3);

#ifdef BOOST_GEOMETRY_POINT_ON_SURFACE_COMPLETE
        //// Right (blue/cyan)
        // (mostly commented, makes the picture less clear)
        //mapper.map(right_points, "stroke:rgb(0,0,255);stroke-width:2");
        //for (intruder_type const& intruder : right_intruders)
        //{
        //    mapper.map(intruder, "stroke:rgb(0,255,255);stroke-width:2");
        //}
        mapper.map(right_point, "opacity:0.8;fill:rgb(0,128,255);stroke:rgb(0,0,100);stroke-width:1", 3);
#endif
    }
#endif

}

template <typename Geometry>
void test_geometry(std::string const& case_id, std::string const& wkt, double expected_x = 0, double expected_y = 0)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    bg::correct(geometry);
    test_geometry(case_id, geometry, expected_x, expected_y);
}

template <typename Point>
void test_point_order_and_type()
{
    typedef bg::model::polygon<Point, false, false> ccw_open_polygon;
    typedef bg::model::polygon<Point,  true, false> cw_open_polygon;
    typedef bg::model::polygon<Point, false,  true> ccw_closed_polygon;
    typedef bg::model::polygon<Point,  true,  true> cw_closed_polygon;

    test_geometry<ccw_open_polygon>("with_hole_ccw_open", "POLYGON((0 0,9 0,9 9,0 9),(2 2,2 7,7 7,7 2))", 0, 0);
    test_geometry<cw_open_polygon>("with_hole_cw_open", "POLYGON((0 0,0 9,9 9,9 0),(2 2,7 2,7 7,2 7))", 0, 0);
    test_geometry<ccw_closed_polygon>("with_hole_ccw_closed", "POLYGON((0 0,9 0,9 9,0 9,0 0),(2 2,2 7,7 7,7 2,2 2))", 0, 0);
    test_geometry<cw_closed_polygon>("with_hole_cw_closed", "POLYGON((0 0,0 9,9 9,9 0,0 0),(2 2,7 2,7 7,2 7,2 2))", 0, 0);

    test_geometry<cw_closed_polygon>("t1", "POLYGON((0 0,0 10,10 0,0 0))", 0, 0);
    test_geometry<cw_closed_polygon>("t2", "POLYGON((0 0,10 0,0 -10,0 0))", 0, 0);
    test_geometry<cw_closed_polygon>("t3", "POLYGON((0 0,0 -10,-10 0,0 0))", 0, 0);
    test_geometry<cw_closed_polygon>("t4", "POLYGON((0 0,-10 0,0 10,0 0))", 0, 0);
}

template <typename Point>
void test_all()
{
    typedef bg::model::polygon<Point> polygon;

    // Specific test-cases for point-on-surface:
    test_geometry<polygon>("ice", "polygon((5 0,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0))", 0, 0);
    test_geometry<polygon>("intruding", "polygon((5 0,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,7 2,4 7,6 1,5 0))", 0, 0);
    test_geometry<polygon>("intruding2", "polygon((5 0,3 2,4 6,2 3,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,7 2,4 7,6 1,5 0))", 0, 0);
    test_geometry<polygon>("intruding3", "polygon((5 0,3 2,3 6,2 3,0 5,1 6,3 7,2 5,4 8,6 5,5 7,7 6,8 5,9 6,10 5,7 2,5 6,6 1,5 0))", 0, 0);
    test_geometry<polygon>("intruding4", "polygon((5 0,3 2,3 6,2 3,0 5,1 6,3 7,2 5,4 8,6 5,5 8,7 6,8 5,9 6,10 5,7 2,5 6,6 1,5 0))", 0, 0);
    test_geometry<polygon>("intruding5", "polygon((5 0,3 2,3 6,2 3,0 5,1 6,3 8,2 5,4 8,6 5,5 8,7 6,8 5,9 6,10 5,7 2,5 6,6 1,5 0))", 0, 0);
    test_geometry<polygon>("ice_int", "polygon((5 0,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(3 4,4 3,5 4,4 7,3 4))", 0, 0);
    test_geometry<polygon>("ice_int2", "polygon((5 0,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(4 7,3 4,4 3,5 4,4 7))", 0, 0);
    test_geometry<polygon>("ice_in3", "polygon((5 0,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(3 4,5 4,5 6,3 6,3 4))", 0, 0);
    test_geometry<polygon>("simplex_normal", simplex_normal[0], 0, 0);
    test_geometry<polygon>("sqr", "polygon((0 0,0 5,0 10,5 10,10 10,10 5,10 0,5 0,0 0))", 0, 0);
    test_geometry<polygon>("self_tangent", "polygon((0 0,5 10,10 0,9 0,7 4,8 0,7 0,5 10,3 0,2 0,3 4,1 0,0 0))", 0, 0);
    test_geometry<polygon>("self_tangent2", "polygon((5 0,2 3,4 8,1.5 3.5,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0))", 0, 0);
    test_geometry<polygon>("self_tangent_int", "polygon((5 0,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(3 4,4 3,5 4,4 8,3 4))", 0, 0);
    test_geometry<polygon>("self_tangent_int2", "polygon((5 0,2 3,3.5 7,1.5 3.5,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(3 4,4 3,5 4,4 8,3 4))", 0, 0);
    test_geometry<polygon>("self_tangent_int3", "polygon((5 0,2 3,4 8,1.5 3.5,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(3 4,4 3,5 4,4 8,3 4))", 0, 0);

    test_geometry<polygon>("self_tangent_int3", "polygon((5 0,2 3,4 8,1.5 3.5,0 5,1 6,2 5,4 8,6 5,7 6,8 5,9 6,10 5,5 0),(3 4,4 3,5 4,4 8,3 4))", 0, 0);
    test_geometry<polygon>("disjoint_simplex0", disjoint_simplex[0], 0, 0);
    test_geometry<polygon>("disjoint_simplex1", disjoint_simplex[1], 0, 0);

    test_geometry<polygon>("ticket_10643", "POLYGON((1074699.93 703064.65, 1074703.90 703064.58, 1074704.53 703061.40, 1074702.10 703054.62, 1074699.93 703064.65))");
    test_geometry<polygon>("ticket_10643_2", "POLYGON((699.93 64.65, 703.90 64.58, 704.53 61.40, 702.10 54.62, 699.93 64.65))");

#if defined(BOOST_GEOMETRY_UNIT_TEST_MULTI)
    {
        typedef bg::model::multi_polygon<polygon> multi_polygon;
        test_geometry<multi_polygon>("mp_simplex", "multipolygon(((0 0,0 5,5 0, 0 0)),((7 1,7 6,10 1,7 1)))", 0, 0);
        // Wrapped polygon in two orders
        test_geometry<multi_polygon>("mp_wrapped1",
                "multipolygon("
                "((4 10,9 11,10 16,11 11,16 10,11 9,10 4,9 9,4 10))"
                ","
                "((0 10,10 20,20 10,10 0,0 10),(10 2,18 10,10 18,2 10,10 2))"
                ")",
                0, 0);
        test_geometry<multi_polygon>("mp_wrapped2",
                "multipolygon("
                "((0 10,10 20,20 10,10 0,0 10),(10 2,18 10,10 18,2 10,10 2))"
                ","
                "((4 10,9 11,10 16,11 11,16 10,11 9,10 4,9 9,4 10))"
                ")",
                0, 0);
    }
#endif


    // Overlay testcases
    test_geometry<polygon>("buffer_mp1", buffer_mp1[0],  0, 0);
    test_geometry<polygon>("buffer_mp2", buffer_mp2[0],  0, 0);
    test_geometry<polygon>("buffer_rt_a", buffer_rt_a[0],  0, 0);
    test_geometry<polygon>("buffer_rt_f", buffer_rt_f[0],  0, 0);
    test_geometry<polygon>("buffer_rt_g", buffer_rt_g[0],  0, 0);
    test_geometry<polygon>("buffer_rt_i", buffer_rt_i[0],  0, 0);
    test_geometry<polygon>("buffer_rt_j", buffer_rt_j[0],  0, 0);
    test_geometry<polygon>("buffer_rt_l", buffer_rt_l[0],  0, 0);
    test_geometry<polygon>("buffer_rt_m1", buffer_rt_m1[0],  0, 0);
    test_geometry<polygon>("buffer_rt_m2", buffer_rt_m2[0],  0, 0);
    test_geometry<polygon>("buffer_rt_n", buffer_rt_n[0],  0, 0);
    test_geometry<polygon>("buffer_rt_q", buffer_rt_q[0],  0, 0);
    test_geometry<polygon>("buffer_rt_r", buffer_rt_r[0],  0, 0);
    test_geometry<polygon>("buffer_rt_t", buffer_rt_t[0],  0, 0);
    test_geometry<polygon>("case_10", case_10[0],  0, 0);
    test_geometry<polygon>("case_11", case_11[0],  0, 0);
    test_geometry<polygon>("case_12", case_12[0],  0, 0);
    test_geometry<polygon>("case_13", case_13[0],  0, 0);
    test_geometry<polygon>("case_14", case_14[0],  0, 0);
    test_geometry<polygon>("case_15", case_15[0],  0, 0);
    test_geometry<polygon>("case_16", case_16[0],  0, 0);
    test_geometry<polygon>("case_17", case_17[0],  0, 0);
    test_geometry<polygon>("case_18", case_18[0],  0, 0);
    test_geometry<polygon>("case_19", case_19[0],  0, 0);
    test_geometry<polygon>("case_1", case_1[0],  0, 0);
    test_geometry<polygon>("case_20", case_20[0],  0, 0);
    test_geometry<polygon>("case_21", case_21[0],  0, 0);
    test_geometry<polygon>("case_22", case_22[0],  0, 0);
    test_geometry<polygon>("case_23", case_23[0],  0, 0);
    test_geometry<polygon>("case_24", case_24[0],  0, 0);
    test_geometry<polygon>("case_25", case_25[0],  0, 0);
    test_geometry<polygon>("case_26", case_26[0],  0, 0);
    test_geometry<polygon>("case_27", case_27[0],  0, 0);
    test_geometry<polygon>("case_28", case_28[0],  0, 0);
    test_geometry<polygon>("case_29", case_29[0],  0, 0);
    test_geometry<polygon>("case_2", case_2[0],  0, 0);
    test_geometry<polygon>("case_30", case_30[0],  0, 0);
    test_geometry<polygon>("case_31", case_31[0],  0, 0);
    test_geometry<polygon>("case_32", case_32[0],  0, 0);
    test_geometry<polygon>("case_33", case_33[0],  0, 0);
    test_geometry<polygon>("case_34", case_34[0],  0, 0);
    test_geometry<polygon>("case_35", case_35[0],  0, 0);
    test_geometry<polygon>("case_36", case_36[0],  0, 0);
    test_geometry<polygon>("case_37", case_37[0],  0, 0);
    test_geometry<polygon>("case_38", case_38[0],  0, 0);
    test_geometry<polygon>("case_39", case_39[0],  0, 0);
    test_geometry<polygon>("case_3", case_3[0],  0, 0);
    test_geometry<polygon>("case_40", case_40[0],  0, 0);
    test_geometry<polygon>("case_41", case_41[0],  0, 0);
    test_geometry<polygon>("case_42", case_42[0],  0, 0);
    test_geometry<polygon>("case_43", case_43[0],  0, 0);
    test_geometry<polygon>("case_44", case_44[0],  0, 0);
    test_geometry<polygon>("case_45", case_45[0],  0, 0);
    test_geometry<polygon>("case_46", case_46[0],  0, 0);
    test_geometry<polygon>("case_47", case_47[0],  0, 0);
    test_geometry<polygon>("case_49", case_49[0],  0, 0);
    test_geometry<polygon>("case_4", case_4[0],  0, 0);
    test_geometry<polygon>("case_50", case_50[0],  0, 0);
    test_geometry<polygon>("case_51", case_51[0],  0, 0);
    test_geometry<polygon>("case_52", case_52[0],  0, 0);
    test_geometry<polygon>("case_53", case_53[0],  0, 0);
    test_geometry<polygon>("case_54", case_54[0],  0, 0);
    test_geometry<polygon>("case_55", case_55[0],  0, 0);
    test_geometry<polygon>("case_56", case_56[0],  0, 0);
    test_geometry<polygon>("case_57", case_57[0],  0, 0);
    test_geometry<polygon>("case_58", case_58[0],  0, 0);
    test_geometry<polygon>("case_59", case_59[0],  0, 0);
    test_geometry<polygon>("case_5", case_5[0],  0, 0);
    test_geometry<polygon>("case_60", case_60[0],  0, 0);
    test_geometry<polygon>("case_61", case_61[0],  0, 0);
    test_geometry<polygon>("case_6", case_6[0],  0, 0);
    test_geometry<polygon>("case_70", case_70[0],  0, 0);
    test_geometry<polygon>("case_71", case_71[0],  0, 0);
    test_geometry<polygon>("case_72", case_72[0],  0, 0);
    test_geometry<polygon>("case_79", case_79[0],  0, 0);
    test_geometry<polygon>("case_7", case_7[0],  0, 0);
    test_geometry<polygon>("case_8", case_8[0],  0, 0);
    test_geometry<polygon>("case_9", case_9[0],  0, 0);
    test_geometry<polygon>("case_many_situations", case_many_situations[0],  0, 0);
    test_geometry<polygon>("ccw_case_1", ccw_case_1[0],  0, 0);
    test_geometry<polygon>("ccw_case_9", ccw_case_9[0],  0, 0);
    test_geometry<polygon>("collinear_opposite_left", collinear_opposite_left[0],  0, 0);
    test_geometry<polygon>("collinear_opposite_right", collinear_opposite_right[0],  0, 0);
    test_geometry<polygon>("collinear_opposite_straight", collinear_opposite_straight[0],  0, 0);
    test_geometry<polygon>("collinear_overlaps", collinear_overlaps[0],  0, 0);
    test_geometry<polygon>("dz_1", dz_1[0],  0, 0);
    test_geometry<polygon>("dz_2", dz_2[0],  0, 0);
    test_geometry<polygon>("dz_3", dz_3[0],  0, 0);
    test_geometry<polygon>("dz_4", dz_4[0],  0, 0);
    test_geometry<polygon>("geos_1", geos_1[0],  0, 0);
    test_geometry<polygon>("geos_2", geos_2[0],  0, 0);
    test_geometry<polygon>("geos_3", geos_3[0],  0, 0);
    test_geometry<polygon>("geos_4", geos_4[0],  0, 0);
    test_geometry<polygon>("ggl_list_20110306_javier", ggl_list_20110306_javier[0],  0, 0);
    test_geometry<polygon>("ggl_list_20110307_javier", ggl_list_20110307_javier[0],  0, 0);
    test_geometry<polygon>("ggl_list_20110627_phillip", ggl_list_20110627_phillip[0],  0, 0);
    test_geometry<polygon>("ggl_list_20110716_enrico", ggl_list_20110716_enrico[0],  0, 0);
    test_geometry<polygon>("ggl_list_20110820_christophe ", ggl_list_20110820_christophe [0],  0, 0);
    test_geometry<polygon>("ggl_list_20120717_volker", ggl_list_20120717_volker[0],  0, 0);
    test_geometry<polygon>("hv_1", hv_1[0],  0, 0);
    test_geometry<polygon>("hv_2", hv_2[0],  0, 0);
    test_geometry<polygon>("hv_3", hv_3[0],  0, 0);
    test_geometry<polygon>("hv_4", hv_4[0],  0, 0);
    test_geometry<polygon>("hv_5", hv_5[0],  0, 0);
    test_geometry<polygon>("hv_6", hv_6[0],  0, 0);
    test_geometry<polygon>("hv_7", hv_7[0],  0, 0);
    test_geometry<polygon>("isovist", isovist[0],  0, 0);
    test_geometry<polygon>("open_case_1", open_case_1[0],  0, 0);
    test_geometry<polygon>("open_case_9", open_case_9[0],  0, 0);
    test_geometry<polygon>("pie_16_2_15_0", pie_16_2_15_0[0],  0, 0);
    test_geometry<polygon>("pie_16_4_12", pie_16_4_12[0],  0, 0);
    test_geometry<polygon>("pie_20_20_7_100", pie_20_20_7_100[0],  0, 0);
    test_geometry<polygon>("pie_23_16_16", pie_23_16_16[0],  0, 0);
    test_geometry<polygon>("pie_23_21_12_500", pie_23_21_12_500[0],  0, 0);
    test_geometry<polygon>("pie_23_23_3_2000", pie_23_23_3_2000[0],  0, 0);
    test_geometry<polygon>("pie_4_13_15", pie_4_13_15[0],  0, 0);
    test_geometry<polygon>("pie_5_12_12_0_7s", pie_5_12_12_0_7s[0],  0, 0);
    test_geometry<polygon>("snl_1", snl_1[0],  0, 0);
    test_geometry<polygon>("ticket_17", ticket_17[0],  0, 0);
    test_geometry<polygon>("ticket_5103", ticket_5103[0],  0, 0);
    test_geometry<polygon>("ticket_7462", ticket_7462[0],  0, 0);
    test_geometry<polygon>("ticket_8310a", ticket_8310a[0],  0, 0);
    test_geometry<polygon>("ticket_8310b", ticket_8310b[0],  0, 0);
    test_geometry<polygon>("ticket_8310c", ticket_8310c[0],  0, 0);
    test_geometry<polygon>("ticket_8254", ticket_8254[0],  0, 0);
}

template <typename Point>
void test_dense(std::string const& case_id, double size)
{
    typedef bg::model::polygon<Point> polygon;
    polygon poly;
    
    bg::append(poly, Point(-size, 0));
    
    double thres = 3.14158 / 8;
    for ( double a = thres ; a > -thres ; a -= 0.01 )
    {
        bg::append(poly, Point(size * ::cos(a), size * ::sin(a)));
    }

    bg::append(poly, Point(-size, 0));

    test_geometry(case_id, poly);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double> >();
    test_point_order_and_type<bg::model::d2::point_xy<double> >();
    test_point_order_and_type<bg::model::d2::point_xy<int> >();

    test_dense<bg::model::d2::point_xy<double> >("dense1", 100);
    test_dense<bg::model::d2::point_xy<double> >("dense2", 1000000);

    return 0;
}

