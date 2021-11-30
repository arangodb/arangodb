// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013, 2014, 2015.
// Modifications copyright (c) 2013-2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_relate.hpp"

//#include <to_svg.hpp>

template <typename P>
void test_polygon_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;

    // touching
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((10 0,10 10,20 10,20 0,10 0))",
                              "FF2F11212");
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((0 -10,0 0,10 0,10 -10,0 -10))",
                              "FF2F11212");
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((10 0,15 10,20 10,20 0,10 0))",
                              "FF2F01212");

    // containing
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 5,5 10,6 10,6 5,5 5))",
                              "212F11FF2");
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 5,5 10,6 5,5 5))",
                              "212F01FF2");
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 5,5 6,6 6,6 5,5 5))",
                              "212FF1FF2");

    // fully containing
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 5,5 9,6 9,6 5,5 5))",
                              "212FF1FF2");
    // fully containing, with a hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(3 3,7 3,7 7,3 7,3 3))",
                              "POLYGON((1 1,1 9,9 9,9 1,1 1))",
                              "2121F12F2");
    // fully containing, both with holes
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(3 3,7 3,7 7,3 7,3 3))",
                              "POLYGON((1 1,1 9,9 9,9 1,1 1),(2 2,8 2,8 8,2 8,2 2))",
                              "212FF1FF2");
    // fully containing, both with holes
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(3 3,7 3,7 7,3 7,3 3))",
                              "POLYGON((1 1,1 9,9 9,9 1,1 1),(4 4,6 4,6 6,4 6,4 4))",
                              "2121F1212");

    // overlapping
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 0,5 10,20 10,20 0,5 0))",
                              "212111212");
    test_geometry<ring, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 0,5 10,20 10,20 0,5 0))",
                              "212111212");
    test_geometry<ring, ring>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((5 0,5 10,20 10,20 0,5 0))",
                              "212111212");
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,15 5,10 0,0 0))",
                              "POLYGON((10 0,5 5,10 10,20 10,20 0,10 0))",
                              "212101212");

    // equal
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((10 10,10 5,10 0,5 0,0 0,0 10,5 10,10 10))",
                              "2FFF1FFF2");
    // hole-sized
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,6 5,6 6,5 6,5 5))",
                              "POLYGON((5 5,5 6,6 6,6 5,5 5))",
                              "FF2F112F2");

    // disjoint
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "POLYGON((0 20,0 30,10 30,10 20,0 20))",
                              "FF2FF1212");
    // disjoint
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(3 3,7 3,7 7,3 7,3 3))",
                              "POLYGON((0 20,0 30,10 30,10 20,0 20))",
                              "FF2FF1212");

    // equal non-simple / non-simple hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,10 5,5 6,5 5))",
                              "POLYGON((0 0,0 10,10 10,10 5,5 6,5 5,10 5,10 0,0 0))",
                              "2FFF1FFF2");

    // within non-simple / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,5 6,5 5,10 5,10 0,0 0))",
                              "POLYGON((0 0,5 5,10 5,10 0,0 0))",
                              "212F11FF2");
    // within non-simple hole / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,10 5,5 6,5 5))",
                              "POLYGON((0 0,5 5,10 5,10 0,0 0))",
                              "212F11FF2");


    // not within non-simple / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,5 6,5 5,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "2FF11F2F2");
    // not within non-simple hole / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,10 5,5 6,5 5))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "2FF11F2F2");
    // not within simple hole / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,9 5,5 6,5 5))",
                              "POLYGON((0 0,0 10,10 10,9 5,10 0,0 0))",
                              "2121112F2");

    // within non-simple fake hole / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,4 7,4 3,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "2FF11F2F2");
    // within non-simple fake hole / non-simple fake hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,4 7,4 3,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 10,10 10,10 5,5 6,5 4,10 5,10 0,0 0))",
                              "2FF11F212");
    // within non-simple fake hole / non-simple hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,4 7,4 3,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,5 6,5 4,10 5))",
                              "2FF11F212");
    // containing non-simple fake hole / non-simple hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,4 7,4 3,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,3 8,3 2,10 5))",
                              "212F11FF2");

    // within non-simple hole / simple
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                              "2FF11F2F2");
    // within non-simple hole / non-simple fake hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 5,5 6,5 4,10 5,10 0,0 0))",
                              "2FF11F212");
    // containing non-simple hole / non-simple fake hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 5,3 8,3 2,10 5,10 0,0 0))",
                              "212F11FF2");
    // equal non-simple hole / non-simple fake hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 5,4 7,4 3,10 5,10 0,0 0))",
                              "2FFF1FFF2");
    // within non-simple hole / non-simple hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,5 6,5 4,10 5))",
                              "2FF11F212");
    // containing non-simple hole / non-simple hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,3 8,3 2,10 5))",
                              "212F11FF2");
    // equal non-simple hole / non-simple hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,4 7,4 3,10 5))",
                              "2FFF1FFF2");

    // intersecting non-simple hole / non-simple hole - touching holes
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,10 5,5 6,5 5))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(0 5,5 4,5 5,0 5))",
                              "21211F2F2");
    // intersecting non-simple fake hole / non-simple hole - touching holes
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,5 6,5 5,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(0 5,5 4,5 5,0 5))",
                              "21211F2F2");
    // intersecting non-simple fake hole / non-simple fake hole - touching holes
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 5,5 6,5 5,10 5,10 0,0 0))",
                              "POLYGON((0 0,0 5,5 4,5 5,0 5,0 10,10 10,10 0,0 0))",
                              "21211F2F2");

    // intersecting simple - i/i
    test_geometry<poly, poly>("POLYGON((0 0,0 10,4 10,6 8,5 5,6 2,4 0,0 0))",
                              "POLYGON((5 5,4 8,6 10,10 10,10 0,6 0,4 2,5 5))",
                              "212101212");
    // intersecting non-simple hole / non-simple hole - i/i
    test_geometry<poly, poly>("POLYGON((0 0,0 10,4 10,6 8,5 5,6 2,4 0,0 0),(5 5,2 6,2 4,5 5))",
                              "POLYGON((5 5,4 8,6 10,10 10,10 0,6 0,4 2,5 5),(5 5,8 4,8 6,5 5))",
                              "212101212");
    // intersecting non-simple hole / simple - i/i
    test_geometry<poly, poly>("POLYGON((0 0,0 10,4 10,6 8,5 5,6 2,4 0,0 0),(5 5,2 6,2 4,5 5))",
                              "POLYGON((5 5,4 8,6 10,10 10,10 0,6 0,4 2,5 5))",
                              "212101212");

    // no turns - disjoint inside a hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,9 1,9 9,1 9,1 1))",
                              "POLYGON((3 3,3 7,7 7,7 3,3 3))",
                              "FF2FF1212");
    // no turns - within
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,9 1,9 9,1 9,1 1))",
                              "POLYGON((-1 -1,-1 11,11 11,11 -1,-1 -1))",
                              "2FF1FF212");
    // no-turns - intersects
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2))",
                              "POLYGON((1 1,1 9,9 9,9 1,1 1))",
                              "2121F12F2");
    // no-turns - intersects, hole in a hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2))",
                              "POLYGON((1 1,1 9,9 9,9 1,1 1),(3 3,7 3,7 7,3 7,3 3))",
                              "2121F1212");

    // no-turns ring - for exteriors
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2))",
                              "POLYGON((1 1,1 9,9 9,9 1,1 1),(2 2,8 2,8 8,2 8,2 2))",
                              "212F11FF2");
    // no-turns ring - for interiors
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(3 3,7 3,7 7,3 7,3 3))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2))",
                              "212F11FF2");

    {
        test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                                  "POLYGON((5 5,5 10,6 10,6 5,5 5))",
                                  "212F11FF2");

        test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                                  "POLYGON((10 0,10 10,20 10,20 0,10 0))",
                                  "FF2F11212");

        namespace bgdr = bg::detail::relate;
        poly p1, p2, p3;
        bg::read_wkt("POLYGON((0 0,0 10,10 10,10 0,0 0))", p1);
        bg::read_wkt("POLYGON((10 0,10 10,20 10,20 0,10 0))", p2);
        bg::read_wkt("POLYGON((5 5,5 10,6 10,6 5,5 5))", p3);
        BOOST_CHECK(bg::relate(p1, p2, bg::de9im::mask("FT*******")
                                    || bg::de9im::mask("F**T*****")
                                    || bg::de9im::mask("F***T****"))); // touches()
        BOOST_CHECK(bg::relate(p1, p3, bg::de9im::mask("T*****FF*"))); // contains()
        BOOST_CHECK(bg::relate(p2, p3, bg::de9im::mask("FF*FF****"))); // disjoint()

        BOOST_CHECK(bg::relate(p1, p2, bg::de9im::static_mask<'F','T'>()
                                    || bg::de9im::static_mask<'F','*','*','T'>()
                                    || bg::de9im::static_mask<'F','*','*','*','T'>()));
    }

    // CCW
    {
        typedef bg::model::polygon<P, false> poly;
        // within non-simple hole / simple
        test_geometry<poly, poly>("POLYGON((0 0,10 0,10 10,0 10,0 0),(5 5,5 6,10 5,5 5))",
                                  "POLYGON((0 0,10 0,10 5,5 5,0 0))",
                                  "212F11FF2");
    }
    // OPEN
    {
        typedef bg::model::polygon<P, true, false> poly;
        // within non-simple hole / simple
        test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0),(5 5,10 5,5 6))",
                                  "POLYGON((0 0,5 5,10 5,10 0))",
                                  "212F11FF2");
    }
    // CCW, OPEN
    {
        typedef bg::model::polygon<P, false, false> poly;
        // within non-simple hole / simple
        test_geometry<poly, poly>("POLYGON((0 0,10 0,10 10,0 10),(5 5,5 6,10 5))",
                                  "POLYGON((0 0,10 0,10 5,5 5))",
                                  "212F11FF2");
    }

    // https://svn.boost.org/trac/boost/ticket/10912
    {
        // external rings touches and G2's hole is inside G1
        test_geometry<poly, poly>("POLYGON((0 0,0 5,5 5,5 0,0 0))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2),(6 6,8 6,8 8,6 8,6 6))",
                                  "21211F212");
        test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,2 4,4 4,4 2,2 2))",
                                  "212F1FFF2");
        // extended
        // external rings touches and G1's hole is inside G2
        test_geometry<poly, poly>("POLYGON((0 0,0 9,9 9,9 0,0 0),(2 2,7 2,7 7,2 7,2 2))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                                  "2FF11F212");
        // external rings touches, holes doesn't
        test_geometry<poly, poly>("POLYGON((0 0,0 9,9 9,9 0,0 0),(2 2,7 2,7 7,2 7,2 2))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,8 1,8 8,1 8,1 1))",
                                  "212111212");
        test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,7 2,7 7,2 7,2 2))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,8 1,8 8,1 8,1 1))",
                                  "212F11FF2");
        // holes touches, external rings doesn't
        test_geometry<poly, poly>("POLYGON((1 1,1 9,9 9,9 1,1 1),(2 2,8 2,8 8,2 8,2 2))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,7 2,7 7,2 7,2 2))",
                                  "2FF11F212");
        test_geometry<poly, poly>("POLYGON((1 1,1 9,9 9,9 1,1 1),(2 2,7 2,7 7,2 7,2 2))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2))",
                                  "212111212");
        test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,7 2,7 7,2 7,2 2))",
                                  "2FF11F212");

        test_geometry<poly, poly>("POLYGON((3 3,3 9,9 9,9 3,3 3))",
                                  "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2))",
                                  "212101212");
    }
    
    if ( BOOST_GEOMETRY_CONDITION((
            boost::is_same<typename bg::coordinate_type<P>::type, double>::value )) )
    {
        // original - assertion for CCW
        //"POLYGON((-0.593220338983050821113352 -8.05084745762711939676137,1.14285714285714279370154 -4,1.50731707317073171381594 1.10243902439024443751237,1.73758865248226967992196 1.37588652482269591104114,1.21739130434782616418943 -3.82608695652173924628414,2 -2,2 1.68750000000000044408921,2.35384615384615436539661 2.10769230769230775379697,2 2.16666666666666651863693,2 4,1.81967213114754100544701 2.1967213114754100544701,1.5882352941176469673934 2.2352941176470588757752,1.8148148148148146585612 5.4074074074074074403029,-0.538461538461538546940233 4.23076923076923083755219,-1.76510067114094004736558 2.89261744966443012927471,-1.64864864864864868465588 2.7567567567567570208098,-1.83962264150943455298659 2.81132075471698161805989,-1.84337349397590433142113 2.80722891566265086993326,-2.14285714285714279370154 2.85714285714285720629846,-2.11111111111111116045436 2.88888888888888883954564,-2.87234042553191448732264 3.10638297872340407579372,-2.91803278688524558859285 3.4262295081967208965068,-3.1733333333333324510761 3.26666666666666660745477,-2.99999999999999822364316 3.14285714285714234961233,-3.25490196078431326398572 3.21568627450980359938626,-3.47368421052631504153396 3.07894736842105265495206,-7.32000000000000028421709 3.72000000000000019539925,-7.54716981132075481752963 3.62264150943396234794136,-7.75 3.79166666666666651863693,-7.79999999999999982236432 3.79999999999999982236432,-7.59999999999999964472863 3.60000000000000008881784,-8.8556701030927822415606 3.06185567010309300783888,-8.82945736434108674473009 2.8914728682170549589614,-7.73333333333333339254523 2.193939393939393855959,-8 2,-5.94736842105263185942476 -1.42105263157894645686952,-5.32558139534883689947264 -0.488372093023255016142059,-5.85714285714285765038767 1.00000000000000066613381,-4.78723404255319184841255 0.319148936170212838003835,-5.32558139534883689947264 -0.488372093023255016142059,-4.74019607843137258385013 -2.12745098039215774221589,-3.17647058823529437887601 -0.705882352941176627325603,-2.93103448275862055183438 -0.862068965517241436735674,-3 -1,-4.57894736842105309904127 -2.57894736842105265495206,-4.47887323943661996850096 -2.85915492957746497637572,-7.58620689655172419918472 -5.18965517241379359347775,-7.52525252525252508206677 -5.5858585858585865224768,-4.18644067796610119813749 -3.67796610169491522412955,-3.44041450777202051369841 -5.76683937823834202873741,-3.73611111111111116045436 -6.56944444444444464181743,-2.8823529411764705621124 -7.7647058823529411242248,-2.88235294117647100620161 -7.7647058823529411242248,-0.593220338983050821113352 -8.05084745762711939676137),(1.66666666666666696272614 1.59999999999999875655021,1.43749999999999911182158 1.8750000000000002220446,0.0869565217391310429917439 2.26086956521739113057379,0.466666666666667118157363 2.60606060606060552231611,1.04878048780487764801705 2.34146341463414664474385,1.43749999999999911182158 1.8750000000000002220446,1.56756756756756754356275 1.83783783783783771781373,1.66666666666666696272614 1.59999999999999875655021))"
        //"POLYGON((-2.33333333333333303727386 -8.66666666666666607454772,-2.26315789473684203514381 -8.63157894736842123961651,-2.88235294117647100620161 -7.7647058823529411242248,-2.8823529411764705621124 -7.7647058823529411242248,-4.11949685534591125701809 -7.61006289308176064878353,-4.32530120481927671249878 -8.16867469879518104391991,-2.33333333333333303727386 -8.66666666666666607454772))"

        // assertion failure in 1.57
        test_geometry<poly, poly>("POLYGON((-0.59322033898305082 -8.0508474576271194,-2.882352941176471 -7.7647058823529411,-2.8823529411764706 -7.7647058823529411,-3.7361111111111112 -6.5694444444444446,-3.4404145077720205 -5.766839378238342,-4.1864406779661012 -3.6779661016949152,-7.5252525252525251 -5.5858585858585865,-7.5862068965517242 -5.1896551724137936,-4.47887323943662 -2.859154929577465,-4.5789473684210531 -2.5789473684210527,-3 -1,-2.9310344827586206 -0.86206896551724144,-3.1764705882352944 -0.70588235294117663,-4.7401960784313726 -2.1274509803921577,-5.3255813953488369 -0.48837209302325502,-4.7872340425531918 0.31914893617021284,-5.8571428571428577 1.0000000000000007,-5.3255813953488369 -0.48837209302325502,-5.9473684210526319 -1.4210526315789465,-8 2,-7.7333333333333334 2.1939393939393939,-8.8294573643410867 2.891472868217055,-8.8556701030927822 3.061855670103093,-7.5999999999999996 3.6000000000000001,-7.7999999999999998 3.7999999999999998,-7.75 3.7916666666666665,-7.5471698113207548 3.6226415094339623,-7.3200000000000003 3.7200000000000002,-3.473684210526315 3.0789473684210527,-3.2549019607843133 3.2156862745098036,-2.9999999999999982 3.1428571428571423,-3.1733333333333325 3.2666666666666666,-2.9180327868852456 3.4262295081967209,-2.8723404255319145 3.1063829787234041,-2.1111111111111112 2.8888888888888888,-2.1428571428571428 2.8571428571428572,-1.8433734939759043 2.8072289156626509,-1.8396226415094346 2.8113207547169816,-1.6486486486486487 2.756756756756757,-1.76510067114094 2.8926174496644301,-0.53846153846153855 4.2307692307692308,1.8148148148148147 5.4074074074074074,1.588235294117647 2.2352941176470589,1.819672131147541 2.1967213114754101,2 4,2 2.1666666666666665,2.3538461538461544 2.1076923076923078,2 1.6875000000000004,2 -2,1.2173913043478262 -3.8260869565217392,1.7375886524822697 1.3758865248226959,1.5073170731707317 1.1024390243902444,1.1428571428571428 -4,-0.59322033898305082 -8.0508474576271194),(1.666666666666667 1.5999999999999988,1.5675675675675675 1.8378378378378377,1.4374999999999991 1.8750000000000002,1.0487804878048776 2.3414634146341466,0.46666666666666712 2.6060606060606055,0.086956521739131043 2.2608695652173911,1.4374999999999991 1.8750000000000002,1.666666666666667 1.5999999999999988))",
                                  "POLYGON((-2.333333333333333 -8.6666666666666661,-4.3253012048192767 -8.168674698795181,-4.1194968553459113 -7.6100628930817606,-2.8823529411764706 -7.7647058823529411,-2.882352941176471 -7.7647058823529411,-2.263157894736842 -8.6315789473684212,-2.333333333333333 -8.6666666666666661))",
                                  "*********");
        // simpler case
        test_geometry<poly, poly>("POLYGON((0 0, -0.59322033898305082 -8.0508474576271194, -2.8823529411764710 -7.7647058823529411, -3.7361111111111112 -6.5694444444444446, 0 0))",
                                  "POLYGON((-10 -10, -4.1194968553459113 -7.6100628930817606, -2.8823529411764706 -7.7647058823529411, -2.2631578947368420 -8.6315789473684212, -10 -10))",
                                  "FF2F01212");
        // sanity check
        test_geometry<poly, poly>("POLYGON((0 0, -0.59322033898305082 -8.0508474576271194, -2.8823529411764710 -7.7647058823529411, -3.7361111111111112 -6.5694444444444446, 0 0))",
                                  "POLYGON((-10 -10, -4.1194968553459113 -7.6100628930817606, -2.8823529411764710 -7.7647058823529411, -2.2631578947368420 -8.6315789473684212, -10 -10))",
                                  "FF2F01212");

        // compatibility check
        /*test_geometry<poly, poly>("POLYGON((0 0,0 1,1 1,1 0,0 0))",
                                  "POLYGON((1 1,1 2,2 2,2 1,1 1))",
                                  "****0****");
        test_geometry<P, P>("POINT(0.9999999999999998 0.9999999999999998)",
                            "POINT(1 1)",
                            "0********");
        test_geometry<P, P>("POINT(0.9999999999999995 0.9999999999999995)",
                            "POINT(1 1)",
                            "F********");
        test_geometry<poly, poly>("POLYGON((0 0,0 1,0.9999999999999998 0.9999999999999998,1 0,0 0))",
                                  "POLYGON((1 1,1 2,2 2,2 1,1 1))",
                                  "****0****");
        test_geometry<poly, poly>("POLYGON((0 0,0 1,0.9999999999999995 0.9999999999999995,1 0,0 0))",
                                  "POLYGON((1 1,1 2,2 2,2 1,1 1))",
                                  "****F****");*/
    }

    // mysql 21872795 (overlaps)
    test_geometry<poly, poly>("POLYGON((2 2,2 8,8 8,8 2,2 2))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(8 8,4 6,4 4,8 8))",
                              "21210F212");
    test_geometry<poly, poly>("POLYGON((2 2,2 8,8 8,8 2,2 2))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 4,4 6,2 2))",
                              "21210F212");
    // mysql 21873343 (touches)
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0), (0 8, 8 5, 8 8, 0 8))",
                              "POLYGON((0 8,-8 5,-8 8,0 8))",
                              "FF2F01212");
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0), (0 6, 6 3, 6 6, 0 6))",
                              "POLYGON((0 6,-6 3,-6 6,0 6))",
                              "FF2F01212");
    // similar but touching from the inside of the hole
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0), (0 8, 8 5, 8 8, 0 8))",
                              "POLYGON((0 8,7 7, 7 6,0 8))",
                              "FF2F01212");
}

template <typename P>
void test_polygon_multi_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<poly, mpoly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                               "MULTIPOLYGON(((5 5,5 10,6 10,6 5,5 5)),((0 20,0 30,10 30,10 20,0 20)))",
                               "212F11212");
    test_geometry<ring, mpoly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                               "MULTIPOLYGON(((5 5,5 10,6 10,6 5,5 5)),((0 20,0 30,10 30,10 20,0 20)))",
                               "212F11212");

    test_geometry<mpoly, poly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                               "POLYGON((0 0,0 5,5 5,5 0,0 0))",
                               "212F11FF2");
    test_geometry<poly, mpoly>("POLYGON((0 0,0 5,5 5,5 0,0 0))",
                               "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                               "2FF11F212");
    test_geometry<poly, mpoly>("POLYGON((0 0,0 5,5 5,5 0,0 0))",
                               "MULTIPOLYGON(((0 0,0 -10,-10 -10,-10 0,0 0)),((0 0,0 10,10 10,10 0,0 0)))",
                               "2FF11F212");
    test_geometry<poly, mpoly>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                               "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                               "2FFF1F212");
}

template <typename P>
void test_multi_polygon_multi_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<mpoly, mpoly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                                "MULTIPOLYGON(((5 5,5 10,6 10,6 5,5 5)),((0 20,0 30,10 30,10 20,0 20)))",
                                "212F11212");
    test_geometry<mpoly, mpoly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 20,0 30,10 30,10 20,0 20)))",
                                "MULTIPOLYGON(((5 5,5 10,6 10,6 5,5 5)))",
                                "212F11FF2");

    test_geometry<mpoly, mpoly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                                "MULTIPOLYGON(((5 5,5 6,6 6,6 5,5 5)),((0 20,0 30,10 30,10 20,0 20)))",
                                "212FF1212");
    test_geometry<mpoly, mpoly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 20,0 30,10 30,10 20,0 20)))",
                                "MULTIPOLYGON(((5 5,5 6,6 6,6 5,5 5)))",
                                "212FF1FF2");
}

template <typename P>
void test_all()
{
    test_polygon_polygon<P>();
    test_polygon_multi_polygon<P>();
    test_multi_polygon_multi_polygon<P>();
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
