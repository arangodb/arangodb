// Boost.Geometry
// Unit Test

// Copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_get_turns.hpp"
#include <boost/geometry/geometries/geometries.hpp>


template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::spherical_equatorial<bg::degree> > pt;
    typedef bg::model::linestring<pt> ls;
    typedef bg::model::multi_linestring<ls> mls;

    // NOTE: currently for the first endpoint of the Linestring on collinear segment
    // is_collinear flags are set to FALSE!
    // E.g. in the first test tii++, NOT tii==

    test_geometry<ls, ls>("LINESTRING(0 0,2 0)", "LINESTRING(0 0,2 0)", "tii++", "txx==");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0)", "LINESTRING(2 0,0 0)", "tix+=", "txi=+");
    
    test_geometry<ls, ls>("LINESTRING(1 0,1 1)", "LINESTRING(0 0,1 0,2 0)", "tuu++");
    test_geometry<ls, ls>("LINESTRING(1 0,0 0)", "LINESTRING(0 0,1 0,2 0)", "txi=+", "tiu+=");
    test_geometry<ls, ls>("LINESTRING(1 0,2 0)", "LINESTRING(0 0,1 0,2 0)", "tii++", "txx==");
    test_geometry<ls, ls>("LINESTRING(1 1,1 0)", "LINESTRING(0 0,1 0,2 0)", "txu++");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0)", "LINESTRING(0 0,1 0,2 0)", "tii++", "txu==");
    test_geometry<ls, ls>("LINESTRING(2 0,1 0)", "LINESTRING(0 0,1 0,2 0)", "txi=+", "tix+=");

    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 0)", "LINESTRING(1 0,1 1)", "tuu++");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 0)", "LINESTRING(1 0,0 0)", "tix+=", "tui=+");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 0)", "LINESTRING(1 0,2 0)", "tii++", "txx==");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 0)", "LINESTRING(1 1,1 0)", "tux++");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 0)", "LINESTRING(0 0,1 0)", "tii++", "tux==");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 0)", "LINESTRING(2 0,1 0)", "tix+=", "txi=+");
    
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,4 0)", "LINESTRING(1 1,1 0,3 0,3 1)", "mii++", "ccc==", "muu==");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,4 0)", "LINESTRING(1 -1,1 0,3 0,3 -1)", "mii++", "ccc==", "muu==");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,4 0)", "LINESTRING(3 1,3 0,1 0,1 1)", "miu+=", "mui=+");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,4 0)", "LINESTRING(3 -1,3 0,1 0,1 -1)", "miu+=", "mui=+");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,3 0,4 0,6 0)", "LINESTRING(2 1,2 0,4 0,4 1)", "tii++", "ccc==", "tuu==");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,3 0,4 0,6 0)", "LINESTRING(2 -1,2 0,4 0,4 -1)", "tii++", "ccc==", "tuu==");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,3 0,4 0,6 0)", "LINESTRING(4 1,4 0,2 0,2 1)", "tiu+=", "tui=+");
    test_geometry<ls, ls>("LINESTRING(0 0,2 0,3 0,4 0,6 0)", "LINESTRING(4 -1,4 0,2 0,2 -1)", "tiu+=", "tui=+");
    
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 1,3 5,4 0)", "LINESTRING(1 0,2 1,3 5)", "tii++", "ecc==", "tux==");
    test_geometry<ls, ls>("LINESTRING(0 0,1 0,2 1,3 5,4 0)", "LINESTRING(3 5,2 1,1 0)", "tix+=", "ecc==", "tui=+");
    test_geometry<ls, ls>("LINESTRING(1 0,2 1,3 5)", "LINESTRING(0 0,1 0,2 1,3 5,4 0)", "txu==", "ecc==", "tii++");
    test_geometry<ls, ls>("LINESTRING(3 5,2 1,1 0)", "LINESTRING(0 0,1 0,2 1,3 5,4 0)", "tiu+=", "ecc==", "txi=+");
    
    test_geometry<ls, ls>("LINESTRING(0 0,10 0)", "LINESTRING(-1 -1,1 0,10 0,20 -1)", "mii++", "txu==");
    test_geometry<ls, ls>("LINESTRING(0 0,10 0)", "LINESTRING(20 -1,10 0,1 0,-1 -1)", "miu+=", "txi=+"); 
    test_geometry<ls, ls>("LINESTRING(-1 -1,1 0,10 0,20 -1)", "LINESTRING(0 0,10 0)", "mii++", "tux==");
    test_geometry<ls, ls>("LINESTRING(20 -1,10 0,1 0,-1 -1)", "LINESTRING(0 0,10 0)", "mui=+", "tix+="); 
    
    test_geometry<ls, ls>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)",
                          "LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)",
                          expected("tii++")("ecc==")("muu==")("mii++")("muu==")("mii++")("mux=="));
    test_geometry<ls, ls>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)",
                          "LINESTRING(30 0,3 0,2.5 1,2 0,1 0,0 0,-1 -1)",
                          expected("tiu+=")("ecc==")("mui=+")("miu+=")("mui=+")("miu+=")("mui=+"));
    test_geometry<ls, ls>("LINESTRING(31 0,15 0,10 5,5 5,4 0,1 0,0 0,-1 1)",
                          "LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)",
                          expected("tui=+")("ecc==")("miu+=")("mui=+")("miu+=")("mui=+")("mix+="));
    test_geometry<ls, ls>("LINESTRING(31 0,15 0,10 5,5 5,4 0,1 0,0 0,-1 1)",
                          "LINESTRING(30 0,3 0,2.5 1,2 0,1 0,0 0,-1 -1)",
                          expected("tuu==")("ecc==")("mii++")("muu==")("mii++")("muu==")("mii++"));

    if (BOOST_GEOMETRY_CONDITION((boost::is_same<T, double>::value)))
    {
        test_geometry<ls, ls>("LINESTRING(-1 0,1 0,2 1.0004570537241201524198894179384922,3 2)",
                              "LINESTRING(4 5,3 2,1 0,0 0)",
                              "mix+=", "txi=+", "ecc==");
        test_geometry<ls, ls>("LINESTRING(4 5,3 2,1 0,0 0)",
                              "LINESTRING(-1 0,1 0,2 1.0004570537241201524198894179384922,3 2)",
                              "mxi=+", "tix+=", "ecc==");
    }
    
    test_geometry<ls, ls>("LINESTRING(30 0,20 0,1 1,-1 -1)", "LINESTRING(0 -1,0 0,1 -1,20 0,25 0)", "mix+=", "tui=+", "muu++");
    test_geometry<ls, ls>("LINESTRING(0 -1,0 0,1 -1,20 0,25 0)", "LINESTRING(30 0,20 0,1 1,-1 -1)", "mxi=+", "tiu+=", "muu++");
    
    test_geometry<ls, ls>("LINESTRING(0 0,30 0)", "LINESTRING(4 0,4 1,20 1,5 0,1 0)", "muu++", "mui=+", "mix+=");
    test_geometry<ls, ls>("LINESTRING(4 0,4 1,20 1,5 0,1 0)", "LINESTRING(0 0,30 0)", "muu++", "miu+=", "mxi=+");
    
    test_geometry<ls, ls>("LINESTRING(30 0,0 0)", "LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0)",
                          expected("mui=+")("miu+=")("mui=+")("mix+="));
    test_geometry<ls, ls>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0)", "LINESTRING(30 0,0 0)",
                          expected("miu+=")("mui=+")("miu+=")("mxi=+"));
    
    test_geometry<ls, ls>("LINESTRING(1 0,7 0,8 1)", "LINESTRING(0 0,10 0,10 10,4 -1)",
                          expected("mii++")("iuu++")("muu=="));
    test_geometry<ls, ls>("LINESTRING(1 0,7 0,8 1)", "LINESTRING(0 0,10 0,10 10,5 0,4 1)",
                          expected("mii++")("muu++")("muu=="));
    
    // non-collinear
    test_geometry<ls, ls>("LINESTRING(0 1,0 0)", "LINESTRING(0 0,1 0,2 0)", "txu++");
    test_geometry<ls, ls>("LINESTRING(0 1,0 0,1 1)", "LINESTRING(0 0,1 0,2 0)", "tuu++");
    test_geometry<ls, ls>("LINESTRING(0 1,1 0,2 1)", "LINESTRING(0 0,1 0,2 0)", "tuu++");

    // SPIKE - NON-ENDPOINT - NON-OPPOSITE

    // spike - neq eq
    test_geometry<ls, ls>("LINESTRING(0 2,0 4,0 1)", "LINESTRING(0 0,0 4,6 3)",
                          expected("mii++")("txu==")("tiu==")("mxi=+"));
    // spike - eq eq
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 1)", "LINESTRING(0 0,0 4,6 3)",
                          expected("tii++")("txu==")("tiu==")("mxi=+"));
    // spike - eq neq
    test_geometry<ls, ls>("LINESTRING(0 0,0 3,0 1)", "LINESTRING(0 0,0 4,6 3)",
                          expected("tii++")("mxu==")("miu==")("mxi=+"));
    // spike - neq neq
    test_geometry<ls, ls>("LINESTRING(0 1,0 3,0 2)", "LINESTRING(0 0,0 4,6 3)",
                          expected("mii++")("mxu==")("miu==")("mxi=+"));
    // spike - out neq
    test_geometry<ls, ls>("LINESTRING(0 0,0 3,0 2)", "LINESTRING(0 1,0 4,6 3)",
                          expected("mii++")("mxu==")("miu==")("mxi=+"));
    // spike - out eq
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(0 1,0 4,6 3)",
                          expected("mii++")("txu==")("tiu==")("mxi=+"));
    // spike - out out/eq
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(1 0,0 4,6 3)",
                          expected("tuu++"));
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(-1 0,0 4,6 3)",
                          expected("tuu++"));
    // spike - out out/neq
    test_geometry<ls, ls>("LINESTRING(0 -2,0 0,0 -1)", "LINESTRING(-1 0,1 0,6 3)",
                          expected("muu++"));
    test_geometry<ls, ls>("LINESTRING(0 -2,0 0,0 -1)", "LINESTRING(-1 0,1 0,6 3)",
                          expected("muu++"));
    
    // SPIKE - NON-ENDPOINT - OPPOSITE
    
    // opposite - eq eq
    test_geometry<ls, ls>("LINESTRING(0 6,0 4,0 0,0 2)", "LINESTRING(0 -1,0 0,0 4,6 3)",
                          expected("tiu+=")("txi=+")("tii=+")("mxu=="));
    test_geometry<ls, ls>("LINESTRING(0 -1,0 0,0 4,6 3)", "LINESTRING(0 6,0 4,0 0,0 2)",
                          expected("tui=+")("tix+=")("tii+=")("mux=="));
    // opposite - neq eq
    test_geometry<ls, ls>("LINESTRING(0 6,0 4,0 0,0 2)", "LINESTRING(0 -1,0 0,0 5,6 3)",
                          expected("miu+=")("txi=+")("tii=+")("mxu=="));
    // opposite - eq neq
    test_geometry<ls, ls>("LINESTRING(0 6,0 4,0 0,0 2)", "LINESTRING(0 -2,0 -1,0 4,6 3)",
                          expected("tiu+=")("mxi=+")("mii=+")("mxu=="));
    // opposite - neq neq
    test_geometry<ls, ls>("LINESTRING(0 6,0 4,0 0,0 2)", "LINESTRING(0 -2,0 -1,0 3,6 3)",
                          expected("miu+=")("mxi=+")("mii=+")("mxu=="));
    // opposite - neq neq
    test_geometry<ls, ls>("LINESTRING(0 6,0 4,0 0,0 2)", "LINESTRING(0 -2,0 -1,0 3,0 5,6 3)",
                          expected("miu+=")("mxi=+")("mii=+")("mxu=="));
    // opposite - neq eq
    test_geometry<ls, ls>("LINESTRING(6 3,0 3,0 0)", "LINESTRING(0 0,0 2,0 3,0 1)",
                          expected("txi=+")("tix+=")("tii+=")("mux=="));
    
    // SPIKE - ENDPOINT - NON-OPPOSITE

    // spike - neq eq
    test_geometry<ls, ls>("LINESTRING(0 2,0 4,0 1)", "LINESTRING(0 0,0 4)",
                          expected("mii++")("txx==")("tix==")("mxi=+"));
    test_geometry<ls, ls>("LINESTRING(0 2,0 4,0 1)", "LINESTRING(0 4,0 0)",
                          expected("miu+=")("txi=+")("tii=+")("mxu=="));
    // spike - eq eq
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 1)", "LINESTRING(0 0,0 4)",
                          expected("tii++")("txx==")("tix==")("mxi=+"));
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 1)", "LINESTRING(0 4,0 0)",
                          expected("tix+=")("txi=+")("tii=+")("mxu=="));
    // spike - eq neq
    test_geometry<ls, ls>("LINESTRING(0 0,0 3,0 1)", "LINESTRING(0 0,0 4)",
                          expected("tii++")("mxu==")("miu==")("mxi=+"));
    test_geometry<ls, ls>("LINESTRING(0 0,0 3,0 1)", "LINESTRING(0 4,0 0)",
                          expected("tix+=")("mxi=+")("mii=+")("mxu=="));
    // spike - neq neq
    test_geometry<ls, ls>("LINESTRING(0 1,0 3,0 2)", "LINESTRING(0 0,0 4)",
                          expected("mii++")("mxu==")("miu==")("mxi=+"));
    test_geometry<ls, ls>("LINESTRING(0 1,0 3,0 2)", "LINESTRING(0 4,0 0)",
                          expected("miu+=")("mxi=+")("mii=+")("mxu=="));
    // spike - out neq
    test_geometry<ls, ls>("LINESTRING(0 0,0 3,0 2)", "LINESTRING(0 1,0 4)",
                          expected("mii++")("mxu==")("miu==")("mxi=+"));
    test_geometry<ls, ls>("LINESTRING(0 0,0 3,0 2)", "LINESTRING(0 4,0 1)",
                          expected("mix+=")("mxi=+")("mii=+")("mxu=="));
    // spike - out eq
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(0 1,0 4)",
                          expected("mii++")("txx==")("tix==")("mxi=+"));
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(0 4,0 1)",
                          expected("mix+=")("txi=+")("tii=+")("mxu=="));
    // spike - out out/eq
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(1 0,0 4)",
                          expected("tux++"));
    test_geometry<ls, ls>("LINESTRING(0 0,0 4,0 2)", "LINESTRING(-1 0,0 4)",
                          expected("tux++"));
    // spike - out out/neq
    test_geometry<ls, ls>("LINESTRING(0 -2,0 0,0 -1)", "LINESTRING(-1 0,1 0)",
                          expected("muu++"));
    test_geometry<ls, ls>("LINESTRING(0 -2,0 0,0 -1)", "LINESTRING(1 0,-1 0)",
                          expected("muu++"));
    

    test_geometry<ls, ls>("LINESTRING(3 0,0 0)",
                          "LINESTRING(4 2,1 0,9 0)",
                          expected("mui=+")("miu+="));


    // 01.02.2015
    test_geometry<ls, ls>("LINESTRING(6 0,0 0,5 0)",
                          "LINESTRING(2 0,0 0,-10 0)",
                          expected("mii++")("txu==")("tiu==")("mui=+"));
    // the reversal could be automatic...
    test_geometry<ls, ls>("LINESTRING(2 0,0 0,-10 0)",
                          "LINESTRING(6 0,0 0,5 0)",
                          expected("mii++")("tux==")("tui==")("miu+="));
    // sanity check
    test_geometry<ls, ls>("LINESTRING(6 0,0 0)",
                          "LINESTRING(2 0,0 0,-10 0)",
                          expected("mii++")("txu=="));
    test_geometry<ls, ls>("LINESTRING(0 0,5 0)",
                          "LINESTRING(2 0,0 0,-10 0)",
                          expected("tiu+=")("mui=+"));
    
    if ( BOOST_GEOMETRY_CONDITION((boost::is_same<T, double>::value)) )
    {
        test_geometry<ls, ls>("LINESTRING(0 -1, 10 -1, 20 1)",
                              "LINESTRING(12 10, 12.5 -0.50051443471392, 15 0, 17.5 0.50051443471392)",
                              expected("mii++")("ccc==")("mux=="));
        test_geometry<ls, ls>("LINESTRING(0 -1, 10 -1, 20 1)",
                              "LINESTRING(17.5 0.50051443471392, 15 0, 12.5 -0.50051443471392, 12 10)",
                              expected("miu+=")("mui=+"));
        test_geometry<ls, ls>("LINESTRING(20 1, 10 -1, 0 -1)",
                              "LINESTRING(12 10, 12.5 -0.50051443471392, 15 0, 17.5 0.50051443471392)",
                              expected("mui=+")("mix+="));
        test_geometry<ls, ls>("LINESTRING(20 1, 10 -1, 0 0)",
                              "LINESTRING(17.5 0.50051443471392, 15 0, 12.5 -0.50051443471392, 12 10)",
                              expected("muu==")("ccc==")("mii++"));
        
        test_geometry<ls, ls>("LINESTRING(0 -1, 10 -1, 20 1)",
                              "LINESTRING(12.5 -0.50051443471392, 15 0, 17.5 0.50051443471392)",
                              expected("mii++")("ccc==")("mux=="));
        test_geometry<ls, ls>("LINESTRING(0 -1, 10 -1, 20 1)",
                              "LINESTRING(17.5 0.50051443471392, 15 0, 12.5 -0.50051443471392)",
                              expected("mix+=")("mui=+"));
        test_geometry<ls, ls>("LINESTRING(20 1, 10 -1, 0 -1)",
                              "LINESTRING(12.5 -0.50051443471392, 15 0, 17.5 0.50051443471392)",
                              expected("mui=+")("mix+="));
        test_geometry<ls, ls>("LINESTRING(20 1, 10 -1, 0 -1)",
                              "LINESTRING(17.5 0.50051443471392, 15 0, 12.5 -0.50051443471392)",
                              expected("mux==")("ccc==")("mii++"));

        test_geometry<ls, ls>("LINESTRING(0 -1, 10 -1, 20 1)",
                              "LINESTRING(12 10, 12.5 -0.50051443471392, 15 0)",
                              expected("mii++")("mux=="));
        test_geometry<ls, ls>("LINESTRING(0 -1, 10 -1, 20 1)",
                              "LINESTRING(15 0, 12.5 -0.50051443471392, 12 10)",
                              expected("miu+=")("mui=+"));
        test_geometry<ls, ls>("LINESTRING(20 1, 10 -1, 0 -1)",
                              "LINESTRING(12 10, 12.5 -0.50051443471392, 15 0)",
                              expected("mui=+")("mix+="));
        test_geometry<ls, ls>("LINESTRING(20 1, 10 -1, 0 -1)",
                              "LINESTRING(15 0, 12.5 -0.50051443471392, 12 10)",
                              expected("muu==")("mii++"));
    }

    // TODO:
    //test_geometry<ls, ls>("LINESTRING(0 0,2 0,1 0)", "LINESTRING(0 1,0 0,2 0)", "1FF00F102");
    //test_geometry<ls, ls>("LINESTRING(2 0,0 0,1 0)", "LINESTRING(0 1,0 0,2 0)", "1FF00F102");

    //test_geometry<ls, ls>("LINESTRING(0 0,3 3,1 1)", "LINESTRING(3 0,3 3,3 1)", "0F1FF0102");
    //test_geometry<ls, ls>("LINESTRING(0 0,3 3,1 1)", "LINESTRING(2 0,2 3,2 1)", "0F1FF0102");
    //test_geometry<ls, ls>("LINESTRING(0 0,3 3,1 1)", "LINESTRING(2 0,2 2,2 1)", "0F1FF0102");

    //test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 3,4 4)", "LINESTRING(0 0,1 1,4 4)", "1FFF0FFF2");


    //if ( boost::is_same<T, double>::value )
    //{
    //    to_svg<ls, ls>("LINESTRING(0 0,1 0,2 0,2.5 0,3 1)", "LINESTRING(0 0,2 0,2.5 0,3 1)", "test11.svg");
    //    to_svg<ls, ls>("LINESTRING(0 0,1 0,2 0,2.5 0,3 1)", "LINESTRING(3 1,2.5 0,2 0,0 0)", "test12.svg");
    //    to_svg<ls, ls>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,20 0,30 0,31 1)", "LINESTRING(30 0,3 0,2.5 1,2 0,1 0,0 0,-1 -1)", "test21.svg");
    //    to_svg<ls, ls>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,20 0,30 0,31 1)", "LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)", "test22.svg");

    //    to_svg<ls, ls>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)", "LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)", "test31.svg");
    //    to_svg<ls, ls>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)", "LINESTRING(30 0,3 0,2.5 1,2 0,1 0,0 0,-1 -1)", "test32.svg");
    //    to_svg<ls, ls>("LINESTRING(31 0,15 0,10 5,5 5,4 0,1 0,0 0,-1 1)", "LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)", "test33.svg");
    //    to_svg<ls, ls>("LINESTRING(31 0,15 0,10 5,5 5,4 0,1 0,0 0,-1 1)", "LINESTRING(30 0,3 0,2.5 1,2 0,1 0,0 0,-1 -1)", "test34.svg");
    //}

    // duplicated
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0,30 0))",
                            "MULTILINESTRING((0 10,5 0,20 0,20 0,30 0),(2 0,2 0),(3 0,3 0,3 0))",
                            expected("mii++")("ccc==")("ccc==")("txx=="));

    // spike
    test_geometry<ls, ls>("LINESTRING(2 2,5 -1,15 2,18 0,20 0)",
                          "LINESTRING(30 0,19 0,18 0,0 0)",
                          expected("iuu++")("iuu++")("tiu+=")("mxi=+"));
    // spike
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0,5 0))",
                            "MULTILINESTRING((1 0,8 0,4 0))",
                            expected("mii++")("mix+=")("mux==")("mui==")("mix+=")("mii+=")("mxu==")("mxi=+"));

    /*test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0,5 0))",
                            "MULTILINESTRING((1 0,8 0,4 0),(2 0,9 0,5 0))",
                            expected("mii")("ccc")("ccc")("txx"));*/

    // spike vs endpoint
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0))",
                            "MULTILINESTRING((-1 0,0 0,-2 0),(11 0,10 0,12 0))",
                            expected("tuu++")("txu++"));
    // internal turning R vs spike
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,2 1))",
                            "MULTILINESTRING((0 1,1 1,0 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,2 1))",
                            "MULTILINESTRING((1 2,1 1,1 2))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 0,0 0))",
                            "MULTILINESTRING((2 0,1 0,2 0))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,2 1))",
                            "MULTILINESTRING((0 2,1 1,0 2))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,2 1))",
                            "MULTILINESTRING((2 0,1 1,2 0))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,2 1))",
                            "MULTILINESTRING((2 1,1 1,2 1))",
                            expected("txi=+")("tix+=")("tii+=")("txx=="));
    // internal turning L vs spike
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,0 1))",
                            "MULTILINESTRING((2 1,1 1,2 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,0 1))",
                            "MULTILINESTRING((1 2,1 1,1 2))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,0 1))",
                            "MULTILINESTRING((2 2,1 1,2 2))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,0 1))",
                            "MULTILINESTRING((0 0,1 1,0 0))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 0,1 1,0 1))",
                            "MULTILINESTRING((0 1,1 1,0 1))",
                            expected("txi=+")("tix+=")("tii+=")("txx=="));
    // spike vs internal turning R
    test_geometry<mls, mls>("MULTILINESTRING((0 1,1 1,0 1))",
                            "MULTILINESTRING((1 0,1 1,2 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 2,1 1,1 2))",
                            "MULTILINESTRING((1 0,1 1,2 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((2 0,1 0,2 0))",
                            "MULTILINESTRING((0 0,1 0,0 0))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 2,1 1,0 2))",
                            "MULTILINESTRING((1 0,1 1,2 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((2 0,1 1,2 0))",
                            "MULTILINESTRING((1 0,1 1,2 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((2 1,1 1,2 1))",
                            "MULTILINESTRING((1 0,1 1,2 1))",
                            expected("tix+=")("txi=+")("tii=+")("txx=="));
    // spike vs internal turning L
    test_geometry<mls, mls>("MULTILINESTRING((2 1,1 1,2 1))",
                            "MULTILINESTRING((1 0,1 1,0 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((1 2,1 1,1 2))",
                            "MULTILINESTRING((1 0,1 1,0 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((2 2,1 1,2 2))",
                            "MULTILINESTRING((1 0,1 1,0 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((1 0,1 1,0 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 1,1 1,0 1))",
                            "MULTILINESTRING((1 0,1 1,0 1))",
                            expected("tix+=")("txi=+")("tii=+")("txx=="));
    // spike vs internal collinear
    test_geometry<mls, mls>("MULTILINESTRING((0 1,1 1,0 1))",
                            "MULTILINESTRING((2 1,1 1,0 1))",
                            expected("tix+=")("txi=+")("tii=+")("txx=="));
    // internal collinear vs spike
    test_geometry<mls, mls>("MULTILINESTRING((2 1,1 1,0 1))",
                            "MULTILINESTRING((0 1,1 1,0 1))",
                            expected("txi=+")("tix+=")("tii+=")("txx=="));
    // spike vs spike
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((2 2,1 1,2 2))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((2 0,1 1,2 0))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((2 1,1 1,2 1))",
                            expected("tuu++"));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((0 1,1 1,0 1))",
                            expected("tuu++"));
    // collinear spikes
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((0 0,1 1,0 0))",
                            expected("tii++")("tix+=")("txi=+")("txx==")
                                    ("ecc=="));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,1 1,0 0))",
                            "MULTILINESTRING((1 1,0 0,1 1))",
                            expected("tix+=")("tii+=")("txx==")("txi==")
                                    ("txi=+")("tii=+")("txx==")("tix=="));
    // non-spike similar
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0))",
                            "MULTILINESTRING((-1 0,0 0,2 0))",
                            expected("tii++")("mux=="));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0))",
                            "MULTILINESTRING((-1 -1,0 0,2 0))",
                            expected("tii++")("mux=="));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0))",
                            "MULTILINESTRING((2 0,0 0,-1 0))",
                            expected("tiu+=")("mui=+"));
    test_geometry<mls, mls>("MULTILINESTRING((0 0,10 0))",
                            "MULTILINESTRING((2 0,0 0,-1 -1))",
                            expected("tiu+=")("mui=+"));
}

int test_main(int, char* [])
{
    test_all<float>();
    test_all<double>();

#if ! defined(_MSC_VER)
    test_all<long double>();
#endif

    return 0;
}
