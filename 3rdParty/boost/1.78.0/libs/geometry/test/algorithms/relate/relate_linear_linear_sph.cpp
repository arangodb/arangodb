// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_relate.hpp"


template <typename P>
void test_linestring_linestring()
{
    using coord_t = typename bg::coordinate_type<P>::type;
    using ls = bg::model::linestring<P>;

    test_geometry<ls, ls>("LINESTRING(0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 3 2)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(0 0,3 2)", "LINESTRING(0 0, 2 2, 3 2)", "FF1F0F1F2");
    test_geometry<ls, ls>("LINESTRING(1 0,2 2,2 3)", "LINESTRING(0 0, 2 2, 3 2)", "0F1FF0102");

    test_geometry<ls, ls>("LINESTRING(0 0,5 0)", "LINESTRING(0 0,1 1,2 0,2 -1)", "0F1F00102");
    
    test_geometry<ls, ls>("LINESTRING(0 0, 1 1.0004570537241206, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 3 2)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(3 2, 2 2, 1 1.0004570537241206, 0 0)", "LINESTRING(0 0, 2 2, 3 2)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(0 0, 1 1.0004570537241206, 2 2, 3 2)", "LINESTRING(3 2, 2 2, 0 0)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(3 2, 2 2, 1 1.0004570537241206, 0 0)", "LINESTRING(3 2, 2 2, 0 0)", "1FFF0FFF2");

    test_geometry<ls, ls>("LINESTRING(3 1, 2 2, 1 1.0004570537241206, 0 0)", "LINESTRING(0 0, 2 2, 3 2)", "1F1F00102");
    test_geometry<ls, ls>("LINESTRING(3 3, 2 2.0015234344160047, 1 1.0012195839797347, 0 0)", "LINESTRING(0 0, 2 2.0015234344160047, 3 2)", "1F1F00102");

    test_geometry<ls, ls>("LINESTRING(0 0, 1 1.0004570537241206, 2 2, 2 3)", "LINESTRING(0 0, 2 2, 2 3)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(2 3, 2 2, 1 1.0004570537241206, 0 0)", "LINESTRING(0 0, 2 2, 2 3)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(0 0, 1 1.0004570537241206, 2 2, 2 3)", "LINESTRING(2 3, 2 2, 0 0)", "1FFF0FFF2");
    test_geometry<ls, ls>("LINESTRING(2 3, 2 2, 1 1.0004570537241206, 0 0)", "LINESTRING(2 3, 2 2, 0 0)", "1FFF0FFF2");

    test_geometry<ls, ls>("LINESTRING(1 1.0004570537241206, 2 2, 3 2.0003044086155000)", "LINESTRING(0 0, 2 2, 4 2)", "1FF0FF102");

    test_geometry<ls, ls>("LINESTRING(3 2.0003044086155000, 2 2, 1 1.0004570537241206)", "LINESTRING(0 0, 2 2, 4 2)", "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 1.0004570537241206, 2 2, 3 2.0003044086155000)", "LINESTRING(4 2, 2 2, 0 0)", "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(3 2.0003044086155000, 2 2, 1 1.0004570537241206)", "LINESTRING(4 2, 2 2, 0 0)", "1FF0FF102");

//    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 2 2)", "LINESTRING(0 0, 2 2, 4 2)", true);

//    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 3)", "LINESTRING(0 0, 2 2, 4 2)", false);
//    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 2, 3 3)", "LINESTRING(0 0, 2 2, 4 2)", false);
//    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 1)", "LINESTRING(0 0, 2 2, 4 2)", false);
//    test_geometry<ls, ls>("LINESTRING(1 1, 2 2, 3 2, 3 1)", "LINESTRING(0 0, 2 2, 4 2)", false);

//    test_geometry<ls, ls>("LINESTRING(0 1, 1 1, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);
//    test_geometry<ls, ls>("LINESTRING(0 1, 0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);
//    test_geometry<ls, ls>("LINESTRING(1 0, 1 1, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);
//    test_geometry<ls, ls>("LINESTRING(1 0, 0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 4 2)", false);

//    test_geometry<ls, ls>("LINESTRING(0 0)", "LINESTRING(0 0)", false);
//    test_geometry<ls, ls>("LINESTRING(1 1)", "LINESTRING(0 0, 2 2)", true);
//    test_geometry<ls, ls>("LINESTRING(0 0)", "LINESTRING(0 0, 2 2)", false);
//    test_geometry<ls, ls>("LINESTRING(0 0, 1 1)", "LINESTRING(0 0)", false);

//    test_geometry<ls, ls>("LINESTRING(0 0,5 0,3 0,6 0)", "LINESTRING(0 0,6 0)", true);
//    test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,1 1)", "LINESTRING(0 0,3 3,6 3)", true);

    // SPIKES!
    test_geometry<ls, ls>("LINESTRING(0 0,2 2.0015234344160047,3 3,1 1.0012195839797347,5 3.0009124369434743)", "LINESTRING(0 0,3 3,6 3)", "1F100F102");
    test_geometry<ls, ls>("LINESTRING(5 3.0009124369434743,1 1.0012195839797347,3 3,2 2.0015234344160047,0 0)", "LINESTRING(0 0,3 3,6 3)", "1F100F102");
    test_geometry<ls, ls>("LINESTRING(0 0,2 2.0015234344160047,3 3,1 1.0012195839797347,5 3.0009124369434743)", "LINESTRING(6 3,3 3,0 0)", "1F100F102");
    test_geometry<ls, ls>("LINESTRING(5 3.0009124369434743,1 1.0012195839797347,3 3,2 2.0015234344160047,0 0)", "LINESTRING(6 3,3 3,0 0)", "1F100F102");

    test_geometry<ls, ls>("LINESTRING(6 3,3 3,0 0)", "LINESTRING(0 0,2 2.0015234344160047,3 3,1 1.0012195839797347,5 3.0009124369434743)", "101F001F2");

    test_geometry<ls, ls>("LINESTRING(0 0,10 0)", "LINESTRING(1 0,9 0,2 0)", "101FF0FF2");
    test_geometry<ls, ls>("LINESTRING(0 0,2 2.0015234344160047,3 3,1 1.0012195839797347)", "LINESTRING(0 0,3 3,6 3)", "1FF00F102");
    // TODO: REWRITE MATRICES
    // BEGIN
    /*test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,1 1)", "LINESTRING(0 0,4 4,6 3)", "1FF00F102");

    test_geometry<ls, ls>("LINESTRING(0 0,2 0,1 0)", "LINESTRING(0 1,0 0,2 0)", "1FF00F102");
    test_geometry<ls, ls>("LINESTRING(2 0,0 0,1 0)", "LINESTRING(0 1,0 0,2 0)", "1FF00F102");

    test_geometry<ls, ls>("LINESTRING(0 0,3 3,1 1)", "LINESTRING(3 0,3 3,3 1)", "0F1FF0102");
    test_geometry<ls, ls>("LINESTRING(0 0,3 3,1 1)", "LINESTRING(2 0,2 3,2 1)", "0F1FF0102");
    test_geometry<ls, ls>("LINESTRING(0 0,3 3,1 1)", "LINESTRING(2 0,2 2,2 1)", "0F1FF0102");
 
     test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,4 4)", "LINESTRING(0 0,1 1,4 4)", "1FFF0FFF2");*/
    // END

    test_geometry<ls, ls>("LINESTRING(0 0,2 2.0036594926050868,3 3.0031983963093536,4 4)", "LINESTRING(0 0,1 1.0022887548647632,4 4)", "1FFF0FFF2");

    // loop i/i i/i u/u u/u
    test_geometry<ls, ls>("LINESTRING(0 0,10 0)",
                          "LINESTRING(1 1,1 0,6 0,6 1,4 1,4 0,9 0,9 1)", "1F1FF0102");

    // self-intersecting and self-touching equal
    test_geometry<ls, ls>("LINESTRING(0 5,5 5,10 5,10 10,5 10,5 5,5 0)",
                          "LINESTRING(0 5,5 5,5 10,10 10,10 5,5 5,5 0)", "1FFF0FFF2");
    // self-intersecting loop and self-touching equal
    test_geometry<ls, ls>("LINESTRING(0 5,5 5,10 5,10 10,5 10,5 5,10 5,10 10,5 10,5 5,5 0)",
                          "LINESTRING(0 5,5 5,5 10,10 10,10 5,5 5,5 0)", "1FFF0FFF2");

    test_geometry<ls, ls>("LINESTRING(0 0,1 1)", "LINESTRING(0 1,1 0)", "0F1FF0102");
    test_geometry<ls, ls>("LINESTRING(0 0,1 1)", "LINESTRING(1 1,2 0)", "FF1F00102");
    test_geometry<ls, ls>("LINESTRING(0 0,1 1)", "LINESTRING(2 0,1 1)", "FF1F00102");

    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 1,3 5,4 0)", "LINESTRING(1 0,2 1,3 5)", "101FF0FF2");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 1,3 5,4 0)", "LINESTRING(3 5,2 1,1 0)", "101FF0FF2");
    test_geometry<ls, ls>("LINESTRING(1 0,2 1,3 5)", "LINESTRING(4 0,3 5,2 1,1 0,0 0)", "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(3 5,2 1,1 0)", "LINESTRING(4 0,3 5,2 1,1 0,0 0)", "1FF0FF102");
    
    test_geometry<ls, ls>("LINESTRING(0 0,10 0)", "LINESTRING(-1 -1,1 0,10 0,20 -1)", "1F10F0102");
    test_geometry<ls, ls>("LINESTRING(0 0,10 0)", "LINESTRING(20 -1,10 0,1 0,-1 -1)", "1F10F0102");

    test_geometry<ls, ls>("LINESTRING(-1 1,0 0,1 0,5 0,5 5,10 5,15 0,31 0)",
                          "LINESTRING(-1 -1,0 0,1 0,2 0,3 1,4 0,30 0)",
                          "101FF0102");
    test_geometry<ls, ls>("LINESTRING(-1 1,0 0,1 0,5 0,5 5,10 5,15 0,31 0)",
                          "LINESTRING(30 0,4 0,3 1,2 0,1 0,0 0,-1 -1)",
                          "101FF0102");
    test_geometry<ls, ls>("LINESTRING(31 0,15 0,10 5,5 5,5 0,1 0,0 0,-1 1)",
                          "LINESTRING(-1 -1,0 0,1 0,2 0,3 1,4 0,30 0)",
                          "101FF0102");
    test_geometry<ls, ls>("LINESTRING(31 0,15 0,10 5,5 5,5 0,1 0,0 0,-1 1)",
                          "LINESTRING(30 0,4 0,3 1,2 0,1 0,0 0,-1 -1)",
                          "101FF0102");

    // self-IP
    test_geometry<ls, ls>("LINESTRING(1 0,9 0)",
                          "LINESTRING(0 0,10 0,10 10,5 0,0 10)",
                          "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 0,5 0,9 0)",
                          "LINESTRING(0 0,10 0,10 10,5 0,0 10)",
                          "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 0,9 0)",
                          "LINESTRING(0 0,10 0,10 10,5 10,5 -1)",
                          "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 0,9 0)",
                          "LINESTRING(0 0,10 0,5 0,5 5)",
                          "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 0,7 0)", "LINESTRING(0 0,10 0,10 10,4 -1)",
                          "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 0,5 0,7 0)", "LINESTRING(0 0,10 0,10 10,4 -1)",
                          "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 0,7 0,8 1)", "LINESTRING(0 0,10 0,10 10,4 -1)",
                          "1F10F0102");
    test_geometry<ls, ls>("LINESTRING(1 0,5 0,7 0,8 1)", "LINESTRING(0 0,10 0,10 10,4 -1)",
                          "1F10F0102");

    // self-IP going out and in on the same point
    test_geometry<ls, ls>("LINESTRING(2 0,5 0,5 5,6 5,5 0,8 0)", "LINESTRING(1 0,9 0)",
                          "1F10FF102");

    // duplicated points
    test_geometry<ls, ls>("LINESTRING(1 1.0004570537241206, 2 2, 2 2)", "LINESTRING(0 0, 2 2, 4 2)", "1FF0FF102");
    test_geometry<ls, ls>("LINESTRING(1 1.0004570537241206, 1 1.0004570537241206, 2 2)", "LINESTRING(0 0, 2 2, 4 2)", "1FF0FF102");

    // linear ring
    test_geometry<ls, ls>("LINESTRING(0 0,10 0)", "LINESTRING(5 0,9 0,5 5,1 0,5 0)", "1F1FF01F2");
    test_geometry<ls, ls>("LINESTRING(0 0,5 0,10 0)", "LINESTRING(5 0,9 0,5 5,1 0,5 0)", "1F1FF01F2");
    test_geometry<ls, ls>("LINESTRING(0 0,5 0,10 0)", "LINESTRING(5 0,10 0,5 5,1 0,5 0)", "1F10F01F2");

    test_geometry<ls, ls>("LINESTRING(0 0,5 0)", "LINESTRING(5 0,10 0,5 5,0 0,5 0)", "1FF0FF1F2");
    test_geometry<ls, ls>("LINESTRING(0 0,5 0)", "LINESTRING(5 0,10 0,5 5,5 0)", "FF10F01F2");

    test_geometry<ls, ls>("LINESTRING(1 0,1 6)", "LINESTRING(0 0,5 0,5 5,0 5)", "0F10F0102");

    // point-size Linestring
    test_geometry<ls, ls>("LINESTRING(1 0,1 0)", "LINESTRING(0 0,5 0)", "0FFFFF102");
    test_geometry<ls, ls>("LINESTRING(1 0,1 0)", "LINESTRING(1 0,5 0)", "F0FFFF102");
    test_geometry<ls, ls>("LINESTRING(1 0,1 0)", "LINESTRING(0 0,1 0)", "F0FFFF102");
    test_geometry<ls, ls>("LINESTRING(1 0,1 0)", "LINESTRING(1 0,1 0)", "0FFFFFFF2");
    test_geometry<ls, ls>("LINESTRING(1 0,1 0)", "LINESTRING(0 0,0 0)", "FF0FFF0F2");

    //to_svg<ls, ls>("LINESTRING(0 0,5 0)", "LINESTRING(5 0,10 0,5 5,5 0)", "test_relate_00.svg");

    // INVALID LINESTRINGS
    // 1-point LS (a Point) NOT disjoint
    //test_geometry<ls, ls>("LINESTRING(1 0)", "LINESTRING(0 0,5 0)", "0FFFFF102");
    //test_geometry<ls, ls>("LINESTRING(0 0,5 0)", "LINESTRING(1 0)", "0F1FF0FF2");
    //test_geometry<ls, ls>("LINESTRING(0 0,5 0)", "LINESTRING(1 0,1 0,1 0)", "0F1FF0FF2");
    // Point/Point
    //test_geometry<ls, ls>("LINESTRING(0 0)", "LINESTRING(0 0)", "0FFFFFFF2");

    if ( BOOST_GEOMETRY_CONDITION(std::is_floating_point<coord_t>::value) )
    {
        // https://svn.boost.org/trac/boost/ticket/10904
        // very small segments
        test_geometry<ls, ls>("LINESTRING(5.6956521739130430148634331999347 -0.60869565217391330413931882503675,5.5 -0.50000000000000066613381477509392)",
                              "LINESTRING(5.5 -0.50000000000000066613381477509392,5.5 -0.5)",
                              "*********"); // TODO: be more specific with the result
        test_geometry<ls, ls>("LINESTRING(-3.2333333333333333925452279800083 5.5999999999999978683717927196994,-3.2333333333333333925452279800083 5.5999999999999996447286321199499)",
                              "LINESTRING(-3.2333333333333325043668082798831 5.5999999999999996447286321199499,-3.2333333333333333925452279800083 5.5999999999999996447286321199499)",
                              "*********"); // TODO: be more specific with the result
    }

    if ( BOOST_GEOMETRY_CONDITION((std::is_same<coord_t, double>::value)) )
    {
        // detected as collinear
        test_geometry<ls, ls>("LINESTRING(1 -10, 3.069359e+307 3.069359e+307)",
                              "LINESTRING(1 6, 1 0)",
                              "*********"); // TODO: be more specific with the result
    }

    // OTHER MASKS
    {
        namespace bgdr = bg::detail::relate;
        ls ls1, ls2, ls3, ls4;
        bg::read_wkt("LINESTRING(0 0,2 0)", ls1);
        bg::read_wkt("LINESTRING(2 0,4 0)", ls2);
        bg::read_wkt("LINESTRING(1 0,1 1)", ls3);
        bg::read_wkt("LINESTRING(1 0,4 0)", ls4);
        BOOST_CHECK(bg::relate(ls1, ls2, bg::de9im::mask("FT*******")
                                      || bg::de9im::mask("F**T*****")
                                      || bg::de9im::mask("F***T****")));
        BOOST_CHECK(bg::relate(ls1, ls3, bg::de9im::mask("FT*******")
                                      || bg::de9im::mask("F**T*****")
                                      || bg::de9im::mask("F***T****")));
        BOOST_CHECK(bg::relate(ls3, ls1, bg::de9im::mask("FT*******")
                                      || bg::de9im::mask("F**T*****")
                                      || bg::de9im::mask("F***T****")));
        BOOST_CHECK(bg::relate(ls2, ls4, bg::de9im::mask("T*F**F***"))); // within

        BOOST_CHECK(bg::relate(ls1, ls2, bg::de9im::static_mask<'F','T'>()
                                      || bg::de9im::static_mask<'F','*','*','T'>()
                                      || bg::de9im::static_mask<'F','*','*','*','T'>()));
        BOOST_CHECK(bg::relate(ls1, ls2, bg::de9im::mask("FT")
                                      || bg::de9im::mask("F**T")
                                      || bg::de9im::mask("F***T**************")));

        BOOST_CHECK_THROW(bg::relate(ls1, ls2, bg::de9im::mask("A")), bg::invalid_input_exception);
    }

    // spike - boundary and interior on the same point
    test_geometry<ls, ls>("LINESTRING(3 7.0222344894505291, 8 8, 2 6.0297078652914440)", "LINESTRING(5 7.0264720782244758, 10 7, 0 7)", "0010F0102");

    // 22.01.2015
    test_geometry<ls, ls>("LINESTRING(5 5.0276084891434261,10 10)", "LINESTRING(6 6.0348911579043349,3 3)", "1010F0102");
    test_geometry<ls, ls>("LINESTRING(5 5.0146631750126396,2 8)", "LINESTRING(4 6.0155438000959975,7 3)", "1010F0102");
}

template <typename P>
void test_linestring_multi_linestring()
{
    using coord_t = typename bg::coordinate_type<P>::type;
    using ls = bg::model::linestring<P>;
    using mls = bg::model::multi_linestring<ls>;

    // LS disjoint
    test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,2 0),(1 1,2 1))", "101FF0102");
    // linear ring disjoint
    test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,2 0),(1 1,2 1,2 2,1 1))", "101FF01F2");
    // 2xLS forming non-simple linear ring disjoint
    test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,2 0),(1 1,2 1,2 2),(1 1,2 2))", "101FF01F2");

    test_geometry<ls, mls>("LINESTRING(0 0,10 0)",
                           "MULTILINESTRING((1 0,9 0),(9 0,2 0))",
                           "101FF0FF2");

    // rings
    test_geometry<ls, mls>("LINESTRING(0 0,5 0,5 5,0 5,0 0)",
                           "MULTILINESTRING((5 5,0 5,0 0),(0 0,5 0,5 5))",
                           "1FFFFFFF2");
    test_geometry<ls, mls>("LINESTRING(0 0,5 0,5 5,0 5,0 0)",
                           "MULTILINESTRING((5 5,5 0,0 0),(0 0,0 5,5 5))",
                           "1FFFFFFF2");
    // overlapping rings
    test_geometry<ls, mls>("LINESTRING(0 0,5 0,5 5,0 5,0 0)",
                           "MULTILINESTRING((5 5,0 5,0 0),(0 0,5 0,5 5,0 5))",
                           "10FFFFFF2");
    test_geometry<ls, mls>("LINESTRING(0 0,5 0,5 5,0 5,0 0)",
                           "MULTILINESTRING((5 5,5 0,0 0),(0 0,0 5,5 5,5 0))",
                           "10FFFFFF2");

    // INVALID LINESTRINGS
    // 1-point LS (a Point) disjoint
    //test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,2 0),(1 1))", "101FF00F2");
    //test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,2 0),(1 1,1 1))", "101FF00F2");
    //test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,2 0),(1 1,1 1,1 1))", "101FF00F2");
    // 1-point LS (a Point) NOT disjoint
    //test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,9 0),(2 0))", "101FF0FF2");
    //test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,9 0),(2 0,2 0))", "101FF0FF2");
    //test_geometry<ls, mls>("LINESTRING(0 0,10 0)", "MULTILINESTRING((1 0,9 0),(2 0,2 0,2 0))", "101FF0FF2");

    // point-like
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 1 0),(2 0, 2 0))",    // |------|  *
                           "101F00FF2");
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 1 0),(1 0, 1 0))",    // |------*
                           "101F00FF2");
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((5 0, 1 0),(1 0, 1 0))",    //        *-------|
                           "101F00FF2");
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 1 0),(5 0, 5 0))",    // |------|       *
                           "10100FFF2");
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 1 0),(0 0, 0 0))",    // *------|
                           "101000FF2");
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((4 0, 5 0),(5 0, 5 0))",    //         |------*
                           "101000FF2");
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((1 0, 2 0),(0 0, 0 0))",    // *  |------|
                           "1010F0FF2");

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      //   |--------------|
                           "MULTILINESTRING((2 0, 2 0),(2 0, 2 2))",    //            *
                           "001FF0102");                                //            |

    // for consistency
    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 5 0),(0 0, 2 0))",    // |--------------|
                           "10F00FFF2");                                // |------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 5 0),(3 0, 5 0))",    // |--------------|
                           "10F00FFF2");                                //         |------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      // |--------------|
                           "MULTILINESTRING((0 0, 5 0),(0 0, 6 0))",    // |--------------|
                           "1FF00F102");                                // |----------------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      //   |--------------|
                           "MULTILINESTRING((0 0, 5 0),(-1 0, 5 0))",   //   |--------------|
                           "1FF00F102");                                // |----------------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      //   |--------------|
                           "MULTILINESTRING((0 0, 5 0),(-1 0, 6 0))",   //   |--------------|
                           "1FF00F102");                                // |------------------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      //   |--------------|
                           "MULTILINESTRING((0 0, 5 0),(-1 0, 2 0))",   //   |--------------|
                           "10F00F102");                                // |-------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      //   |--------------|
                           "MULTILINESTRING((0 0, 5 0),(2 0, 6 0))",    //   |--------------|
                           "10F00F102");                                //            |-------|

    test_geometry<ls, mls>("LINESTRING(0 0, 5 0)",                      //   |--------------|
                           "MULTILINESTRING((0 0, 5 0),(2 0, 2 2))",    //   |--------------|
                           "10FF0F102");                                //            |
                                                                        //            |

    if ( BOOST_GEOMETRY_CONDITION(std::is_floating_point<coord_t>::value) )
    {
        // related to https://svn.boost.org/trac/boost/ticket/10904
        test_geometry<ls, mls>("LINESTRING(-2305843009213693956 4611686018427387906, -33 -92, 78 83)",
                               "MULTILINESTRING((20 100, 31 -97, -46 57, -20 -4),(-71 -4))",
                               "*********"); // TODO: be more specific with the result
    }

    // 22.01.2015
    // inspired by L/A and A/A
    test_geometry<ls, mls>("LINESTRING(1 1.0012195839797347,2 2.0015234344160047)",
                           "MULTILINESTRING((0 0,1 1.0012195839797347),(1 1.0012195839797347,3 3))",
                           "1FF0FF102");
    // for floats this is considered as collinear due to a special check in spherical intersection strategy
    test_geometry<ls, mls>("LINESTRING(1 1,2 2)",
                           "MULTILINESTRING((0 0,1 1),(1 1,3 3))",
                           "FF10F0102", "1FF0FF102");

    // 25.01.2015
    test_geometry<ls, mls>("LINESTRING(1 1.0152687848942923, 5 5.0575148968282102, 4 4.0516408785782314)",
                           "MULTILINESTRING((2 5.1322576184978592, 7 5, 8 3, 6 3, 4 0),(0 0,10 10))",
                           "1FF0FF102");
    test_geometry<ls, mls>("LINESTRING(4 1, 4 5, 4 4)",
                           "MULTILINESTRING((2 5.1322576184978592, 7 5, 8 3, 6 3, 4 0),(4 0, 4 8, 0 4))",
                           "1FF0FF102");
    test_geometry<ls, mls>("LINESTRING(1 1.0152687848942923,5 5.0575148968282102,4 4.0516408785782314)",
                           "MULTILINESTRING((5 0,5 5.0575148968282102,5 10),(0 0,10 10))",
                           "1FF0FF102");
    test_geometry<ls, mls>("LINESTRING(1 1.0036662021874625,5 5,1 0)",
                           "MULTILINESTRING((5 0,5 5,5 10),(0 0,5 5,1 0))",
                           "1FF00F102");
    test_geometry<ls, mls>("LINESTRING(5 5.0575148968282102,4 4.0516408785782314)",
                           "MULTILINESTRING((5 0,5 5.0575148968282102,5 10),(0 0,10 10))",
                           "1FF0FF102");
    test_geometry<ls, mls>("LINESTRING(6 6.0587459045645184,5 5.0575148968282102,4 4.0516408785782314)",
                           "MULTILINESTRING((5 0,5 5.0575148968282102,5 10),(0 0,10 10))",
                           "1FF0FF102");
    test_geometry<ls, mls>("LINESTRING(5 5,4 4)",
                           "MULTILINESTRING((5 0,5 5,5 10))",
                           "FF10F0102");
}

template <typename P>
void test_multi_linestring_multi_linestring()
{
    using coord_t = typename bg::coordinate_type<P>::type;
    using ls = bg::model::linestring<P>;
    using mls = bg::model::multi_linestring<ls>;

    test_geometry<mls, mls>("MULTILINESTRING((0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0))",
                            "MULTILINESTRING((0 10,5 0,20 0,20 0,30 0))",
                            "1F1F00102");
    test_geometry<mls, mls>("MULTILINESTRING((0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0))",
                            //"MULTILINESTRING((0 10,5 0,20 0,20 0,30 0),(1 10,1 10,1 0,1 0,1 -10),(2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),(30 0,30 0,31 0,31 0))",
                            "MULTILINESTRING((0 10,5 0,20 0,20 0,30 0),(1 10,1 10,1 0,1 0,1 -10),(0 0,0 0,0 10,0 10),(30 0,30 0,31 0,31 0))",
                            "1F100F102");
    test_geometry<mls, mls>("MULTILINESTRING((0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0))",
                            "MULTILINESTRING((0 10,5 0,20 0,20 0,30 0),(0 0,0 0,0 10,0 10))",
                            "1F1F0F1F2");

    // point-like
    test_geometry<mls, mls>("MULTILINESTRING((0 0, 0 0),(1 1, 1 1))",
                            "MULTILINESTRING((0 0, 0 0))",
                            "0F0FFFFF2");
    test_geometry<mls, mls>("MULTILINESTRING((0 0, 0 0),(1 1, 1 1))",
                            "MULTILINESTRING((0 0, 0 0),(1 1, 1 1))",
                            "0FFFFFFF2");
    test_geometry<mls, mls>("MULTILINESTRING((0 0, 0 0),(1 1, 1 1))",
                            "MULTILINESTRING((2 2, 2 2),(3 3, 3 3))",
                            "FF0FFF0F2");

    test_geometry<mls, mls>("MULTILINESTRING((0 5,10 5,10 10,5 10),(5 10,5 0,5 2),(5 2,5 5.0190018174896416,0 5))",
                            "MULTILINESTRING((5 5.0190018174896416,0 5),(5 5.0190018174896416,5 0),(10 10,10 5,5 5.0190018174896416,5 10,10 10))",
                            "10FFFFFF2");

    if ( BOOST_GEOMETRY_CONDITION((std::is_same<coord_t, double>::value)) )
    {
        // assertion failure in 1.57
        test_geometry<mls, mls>("MULTILINESTRING((-0.59322033898305082 -8.0508474576271194,-2.882352941176471 -7.7647058823529411,-2.8823529411764706 -7.7647058823529411,-3.7361111111111112 -6.5694444444444446,-3.4404145077720205 -5.766839378238342,-4.1864406779661012 -3.6779661016949152,-7.5252525252525251 -5.5858585858585865,-7.5862068965517242 -5.1896551724137936,-4.47887323943662 -2.859154929577465,-4.5789473684210531 -2.5789473684210527,-3 -1,-2.9310344827586206 -0.86206896551724144,-3.1764705882352944 -0.70588235294117663,-4.7401960784313726 -2.1274509803921577,-5.3255813953488369 -0.48837209302325502,-4.7872340425531918 0.31914893617021284,-5.8571428571428577 1.0000000000000007,-5.3255813953488369 -0.48837209302325502,-5.9473684210526319 -1.4210526315789465,-8 2,-7.7333333333333334 2.1939393939393939,-8.8294573643410867 2.891472868217055,-8.8556701030927822 3.061855670103093,-7.5999999999999996 3.6000000000000001,-7.7999999999999998 3.7999999999999998,-7.75 3.7916666666666665,-7.5471698113207548 3.6226415094339623,-7.3200000000000003 3.7200000000000002,-3.473684210526315 3.0789473684210527,-3.2549019607843133 3.2156862745098036,-2.9999999999999982 3.1428571428571423,-3.1733333333333325 3.2666666666666666,-2.9180327868852456 3.4262295081967209,-2.8723404255319145 3.1063829787234041,-2.1111111111111112 2.8888888888888888,-2.1428571428571428 2.8571428571428572,-1.8433734939759043 2.8072289156626509,-1.8396226415094346 2.8113207547169816,-1.6486486486486487 2.756756756756757,-1.76510067114094 2.8926174496644301,-0.53846153846153855 4.2307692307692308,1.8148148148148147 5.4074074074074074,1.588235294117647 2.2352941176470589,1.819672131147541 2.1967213114754101,2 4,2 2.1666666666666665,2.3538461538461544 2.1076923076923078,2 1.6875000000000004,2 -2,1.2173913043478262 -3.8260869565217392,1.7375886524822697 1.3758865248226959,1.5073170731707317 1.1024390243902444,1.1428571428571428 -4,-0.59322033898305082 -8.0508474576271194),(1.666666666666667 1.5999999999999988,1.5675675675675675 1.8378378378378377,1.4374999999999991 1.8750000000000002,1.0487804878048776 2.3414634146341466,0.46666666666666712 2.6060606060606055,0.086956521739131043 2.2608695652173911,1.4374999999999991 1.8750000000000002,1.666666666666667 1.5999999999999988))",
                                "MULTILINESTRING((-2.333333333333333 -8.6666666666666661,-4.3253012048192767 -8.168674698795181,-4.1194968553459113 -7.6100628930817606,-2.8823529411764706 -7.7647058823529411,-2.882352941176471 -7.7647058823529411,-2.263157894736842 -8.6315789473684212,-2.333333333333333 -8.6666666666666661))",
                                "*********");
    }
}

template <typename P>
void test_all()
{
    test_linestring_linestring<P>();
    test_linestring_multi_linestring<P>();
    test_multi_linestring_multi_linestring<P>();
}

int test_main( int , char* [] )
{
    typedef bg::cs::spherical_equatorial<bg::degree> cs_t;

    test_all<bg::model::point<float, 2, cs_t> >();
    test_all<bg::model::point<double, 2, cs_t> >();

    return 0;
}
