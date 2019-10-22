// Boost.Geometry
// Unit Test

// Copyright (c) 2016, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_relate.hpp"


template <typename P>
void test_linestring_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;

    // LS disjoint
    test_geometry<ls, poly>("LINESTRING(11 0,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "FF1FF0212");
    test_geometry<ls, ring>("LINESTRING(11 0,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "FF1FF0212");

    // II BB
    test_geometry<ls, poly>("LINESTRING(0 0,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))",    "1FFF0F212");
    test_geometry<ls, poly>("LINESTRING(5 0,5 5,10 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "1FFF0F212");
    test_geometry<ls, poly>("LINESTRING(5 1,5 5,9 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))",  "1FF0FF212");
    
    // IE 
    test_geometry<ls, poly>("LINESTRING(11 1,11 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "FF1FF0212");
    // IE IB0
    test_geometry<ls, poly>("LINESTRING(11 1,10 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "FF1F00212");
    // IE IB1
    test_geometry<ls, poly>("LINESTRING(11 1,10 5,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F00212");
    test_geometry<ls, poly>("LINESTRING(11 1,10 10,0 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F00212");
    test_geometry<ls, poly>("LINESTRING(11 1,10 0,0 0)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F00212");
    test_geometry<ls, poly>("LINESTRING(0 -1,1 0,2 0)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F00212");
    // IE IB0 II
    test_geometry<ls, poly>("LINESTRING(11 1,10 5,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "1010F0212");
    // IE IB0 lring
    test_geometry<ls, poly>("LINESTRING(11 1,10 5,11 5,11 1)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F01FFF212");
    // IE IB1 lring
    test_geometry<ls, poly>("LINESTRING(11 1,10 5,10 10,11 5,11 1)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11FFF212");
    
    // IB1 II
    test_geometry<ls, poly>("LINESTRING(0 0,5 0,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "11F00F212");
    // BI0 II IB1
    test_geometry<ls, poly>("LINESTRING(5 0,5 5,10 5,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "11FF0F212");

    // IB1 II IB1
    test_geometry<ls, poly>("LINESTRING(1 0,2 0,3 1,4 0,5 0)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "11FF0F212");
    // IB1 IE IB1
    test_geometry<ls, poly>("LINESTRING(1 0,2 0,3 -1,4 0,5 0)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F0F212");

    // II IB1
    test_geometry<ls, poly>("LINESTRING(5 5,10 5,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "11F00F212");
    // IB1 II
    test_geometry<ls, poly>("LINESTRING(10 10,10 5,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "11F00F212");
    // IE IB1
    test_geometry<ls, poly>("LINESTRING(15 5,10 5,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F00212");
    // IB1 IE
    test_geometry<ls, poly>("LINESTRING(10 10,10 5,15 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "F11F00212");

    // duplicated points
    {
        // II IB0 IE
        test_geometry<ls, poly>("LINESTRING(5 5,10 5,15 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "1010F0212");
        test_geometry<ls, poly>("LINESTRING(5 5,5 5,5 5,10 5,10 5,10 5,15 10,15 10,15 10)",
            "POLYGON((0 0,0 0,0 0,0 10,0 10,0 10,10 10,10 10,10 10,10 0,10 0,10 0,0 0,0 0,0 0))",
            "1010F0212");
        test_geometry<ls, poly>("LINESTRING(5 5,5 5,5 5,10 0,10 0,10 0,15 10,15 10,15 10)",
            "POLYGON((0 0,0 0,0 0,0 10,0 10,0 10,10 10,10 10,10 10,10 0,10 0,10 0,0 0,0 0,0 0))",
            "1010F0212");
        // IE IB0 II
        test_geometry<ls, poly>("LINESTRING(15 10,15 10,15 10,10 5,10 5,10 5,5 5,5 5,5 5)",
            "POLYGON((0 0,0 0,0 0,0 10,0 10,0 10,10 10,10 10,10 10,10 0,10 0,10 0,0 0,0 0,0 0))",
            "1010F0212");
        test_geometry<ls, poly>("LINESTRING(15 10,15 10,15 10,10 0,10 0,10 0,5 5,5 5,5 5)",
            "POLYGON((0 0,0 0,0 0,0 10,0 10,0 10,10 10,10 10,10 10,10 0,10 0,10 0,0 0,0 0,0 0))",
            "1010F0212");

        // TEST
        //test_geometry<ls, poly>("LINESTRING(5 5,5 5,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "1010F0212");
        test_geometry<ls, poly>("LINESTRING(5 5,5 5,5 5,15 5,15 5,15 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", "1010F0212");
    }

    // non-simple polygon with hole
    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "10F0FF212");
    test_geometry<ls, poly>("LINESTRING(10 1,10 5,10 9)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "F1FF0F212");
    test_geometry<ls, poly>("LINESTRING(2 8,10 5,2 2)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "F1FF0F212");
    test_geometry<ls, poly>("LINESTRING(10 1,10 5,2 2)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "F1FF0F212");
    test_geometry<ls, poly>("LINESTRING(10 1,10 5,2 8)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "F1FF0F212");

    // non-simple polygon with hole, linear ring
    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9,1 9,1 1,9 1)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "10FFFF212");
    test_geometry<ls, poly>("LINESTRING(10 5,10 9,11 5,10 1,10 5)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "F11FFF212");
    test_geometry<ls, poly>("LINESTRING(11 5,10 1,10 5,10 9,11 5)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            "F11FFF212");

    // non-simple polygon with self-touching holes
    test_geometry<ls, poly>("LINESTRING(7 1,8 5,7 9)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(8 1,9 1,9 9,8 9,8 1),(2 2,8 5,2 8,2 2))",
                            "10F0FF212");
    test_geometry<ls, poly>("LINESTRING(8 2,8 5,8 8)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(8 1,9 1,9 9,8 9,8 1),(2 2,8 5,2 8,2 2))",
                            "F1FF0F212");
    test_geometry<ls, poly>("LINESTRING(2 8,8 5,2 2)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(8 1,9 1,9 9,8 9,8 1),(2 2,8 5,2 8,2 2))",
                            "F1FF0F212");

    // non-simple polygon self-touching
    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9)",
                            "POLYGON((0 0,0 10,10 10,10 5,2 8,2 2,10 5,10 0,0 0))",
                            "10F0FF212");
    test_geometry<ls, poly>("LINESTRING(10 1,10 5,10 9)",
                            "POLYGON((0 0,0 10,10 10,10 5,2 8,2 2,10 5,10 0,0 0))",
                            "F1FF0F212");
    test_geometry<ls, poly>("LINESTRING(2 8,10 5,2 2)",
                            "POLYGON((0 0,0 10,10 10,10 5,2 8,2 2,10 5,10 0,0 0))",
                            "F1FF0F212");

    // non-simple polygon self-touching, linear ring
    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9,1 9,1 1,9 1)",
                            "POLYGON((0 0,0 10,10 10,10 5,2 8,2 2,10 5,10 0,0 0))",
                            "10FFFF212");
    test_geometry<ls, poly>("LINESTRING(10 5,10 9,11 5,10 1,10 5)",
                            "POLYGON((0 0,0 10,10 10,10 5,2 8,2 2,10 5,10 0,0 0))",
                            "F11FFF212");
    test_geometry<ls, poly>("LINESTRING(11 5,10 1,10 5,10 9,11 5)",
                            "POLYGON((0 0,0 10,10 10,10 5,2 8,2 2,10 5,10 0,0 0))",
                            "F11FFF212");

    // polygons with some ring equal to the linestring
    test_geometry<ls, poly>("LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                            "F1FFFF2F2");
    test_geometry<ls, poly>("LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,5 5,2 8,2 2))",
                            "F1FFFF212");
    test_geometry<ls, poly>("LINESTRING(2 2,5 5,2 8,2 2)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,5 5,2 8,2 2))",
                            "F1FFFF212");

    // self-IP going on the boundary then into the exterior and to the boundary again
    test_geometry<ls, poly>("LINESTRING(2 10.023946271183535,5 10.037423045910710,5 15,6 15,5 10.037423045910710,8 10.023946271183535)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                            "F11F0F212");
    // self-IP going on the boundary then into the interior and to the boundary again
    test_geometry<ls, poly>("LINESTRING(2 10.023946271183535,5 10.037423045910710,5 5,6 5,5 10.037423045910710,8 10.023946271183535)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                            "11FF0F212");

    // self-IP with a hole -> B to I to B to E
    test_geometry<ls, poly>("LINESTRING(0 0,3 3)", "POLYGON((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0))",
                            "FF1F00212");
}

template <typename P>
void test_linestring_multi_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,10 9)",
                            "MULTIPOLYGON(((0 20,0 30,10 30,10 20,0 20)),((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)))",
                            "F1FF0F212");
    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,10 9)",
                            "MULTIPOLYGON(((0 20,0 30,10 30,10 20,0 20)),((0 0,0 10,10 10,10 0,0 0)))",
                            "F1FF0F212");

    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,2 2)",
                            "MULTIPOLYGON(((0 20,0 30,10 30,10 20,0 20)),((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)))",
                            "F1FF0F212");
    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,2 2)",
                            "MULTIPOLYGON(((0 20,0 30,10 30,10 20,0 20)),((0 0,0 10,10 10,10 0,0 0)))",
                            "11F00F212");

    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,2 2)",
                            "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)),((10 5,3 3,3 7,10 5)))",
                            "F1FF0F212");
    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,2 8)",
                            "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)),((10 5,3 3,3 7,10 5)))",
                            "F1FF0F212");
    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,3 3)",
                            "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)),((10 5,3 3,3 7,10 5)))",
                            "F1FF0F212");
    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,3 7)",
                            "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)),((10 5,3 3,3 7,10 5)))",
                            "F1FF0F212");
    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,5 5)",
                            "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)),((10 5,3 3,3 7,10 5)))",
                            "11F00F212");

    test_geometry<ls, mpoly>("LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((20 0,20 10,30 20,20 0)))",
                             "F1FFFF212");

    // degenerated points
    test_geometry<ls, mpoly>("LINESTRING(5 5,10 10,10 10,10 10,15 15)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((10 10,10 20,20 20,20 10,10 10)))",
                             "10F0FF212");

    // self-IP polygon with a hole and second polygon with a hole -> B to I to B to B to I to B to E
    test_geometry<ls, mpoly>("LINESTRING(0 0,3 3)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0)),((0 0,2 8,8 8,8 2,0 0),(0 0,7 3,7 7,3 7,0 0)))",
                             "FF1F00212");
    // self-IP polygon with a hole and second polygon -> B to I to B to B to I
    test_geometry<ls, mpoly>("LINESTRING(0 0,3 3)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0)),((0 0,2 8,8 8,8 2,0 0)))",
                             "1FF00F212");
    test_geometry<ls, mpoly>("LINESTRING(0 0,3 3)",
                             "MULTIPOLYGON(((0 0,2 8,8 8,8 2,0 0)),((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0)))",
                             "1FF00F212");

    // MySQL report 18.12.2014 (https://svn.boost.org/trac/boost/ticket/10887)
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 2)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                             "10F0FF212");
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 5.0190018174896416)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                             "10F00F212");
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 0)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                             "1FF00F212");
    // MySQL report 18.12.2014 - extended
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 0)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)),((5 0,7 1,7 -1,5 0)))",
                             "1FF00F212");
    test_geometry<ls, mpoly>("LINESTRING(0 0,5 0)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)),((5 0,7 1,7 -1,5 0)))",
                             "FF1F00212");

    // 22.01.2015
    test_geometry<ls, mpoly>("LINESTRING(5 5,0 0,10 0)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                             "11F00F212");
    test_geometry<ls, mpoly>("LINESTRING(5 5,0 0,0 10)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                             "11F00F212");
    // extended
    test_geometry<ls, mpoly>("LINESTRING(5 5,0 0,2 1)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                             "10F0FF212");
    test_geometry<ls, mpoly>("LINESTRING(5 5,0 0,5 -5)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                             "1010F0212");
    test_geometry<ls, mpoly>("LINESTRING(5 5,0 0,5 -5,5 1)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                             "1010FF212");
    test_geometry<ls, mpoly>("LINESTRING(-5 5,0 0,5 -5)",
                             "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                             "F01FF0212");
}

template <typename P>
void test_multi_linestring_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef typename bg::coordinate_type<P>::type coord_t;

    test_geometry<mls, poly>("MULTILINESTRING((11 11, 20 20),(5 7, 4 1))",
                             "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2))",
                             "1F10F0212");
    test_geometry<mls, poly>("MULTILINESTRING((10 0, 18 12),(2 2,2 1))",
                             "POLYGON((5 0,0 -5,-5 0,0 5,5 0))",
                             "1F10F0212");

    if ( BOOST_GEOMETRY_CONDITION((
            boost::is_same<coord_t, double>::value )) )
    {
        // assertion failure in 1.57
        test_geometry<mls, poly>("MULTILINESTRING((-0.59322033898305082 -8.0508474576271194,-2.882352941176471 -7.7647058823529411,-2.8823529411764706 -7.7647058823529411,-3.7361111111111112 -6.5694444444444446,-3.4404145077720205 -5.766839378238342,-4.1864406779661012 -3.6779661016949152,-7.5252525252525251 -5.5858585858585865,-7.5862068965517242 -5.1896551724137936,-4.47887323943662 -2.859154929577465,-4.5789473684210531 -2.5789473684210527,-3 -1,-2.9310344827586206 -0.86206896551724144,-3.1764705882352944 -0.70588235294117663,-4.7401960784313726 -2.1274509803921577,-5.3255813953488369 -0.48837209302325502,-4.7872340425531918 0.31914893617021284,-5.8571428571428577 1.0000000000000007,-5.3255813953488369 -0.48837209302325502,-5.9473684210526319 -1.4210526315789465,-8 2,-7.7333333333333334 2.1939393939393939,-8.8294573643410867 2.891472868217055,-8.8556701030927822 3.061855670103093,-7.5999999999999996 3.6000000000000001,-7.7999999999999998 3.7999999999999998,-7.75 3.7916666666666665,-7.5471698113207548 3.6226415094339623,-7.3200000000000003 3.7200000000000002,-3.473684210526315 3.0789473684210527,-3.2549019607843133 3.2156862745098036,-2.9999999999999982 3.1428571428571423,-3.1733333333333325 3.2666666666666666,-2.9180327868852456 3.4262295081967209,-2.8723404255319145 3.1063829787234041,-2.1111111111111112 2.8888888888888888,-2.1428571428571428 2.8571428571428572,-1.8433734939759043 2.8072289156626509,-1.8396226415094346 2.8113207547169816,-1.6486486486486487 2.756756756756757,-1.76510067114094 2.8926174496644301,-0.53846153846153855 4.2307692307692308,1.8148148148148147 5.4074074074074074,1.588235294117647 2.2352941176470589,1.819672131147541 2.1967213114754101,2 4,2 2.1666666666666665,2.3538461538461544 2.1076923076923078,2 1.6875000000000004,2 -2,1.2173913043478262 -3.8260869565217392,1.7375886524822697 1.3758865248226959,1.5073170731707317 1.1024390243902444,1.1428571428571428 -4,-0.59322033898305082 -8.0508474576271194),(1.666666666666667 1.5999999999999988,1.5675675675675675 1.8378378378378377,1.4374999999999991 1.8750000000000002,1.0487804878048776 2.3414634146341466,0.46666666666666712 2.6060606060606055,0.086956521739131043 2.2608695652173911,1.4374999999999991 1.8750000000000002,1.666666666666667 1.5999999999999988))",
                                 "POLYGON((-2.333333333333333 -8.6666666666666661,-4.3253012048192767 -8.168674698795181,-4.1194968553459113 -7.6100628930817606,-2.8823529411764706 -7.7647058823529411,-2.882352941176471 -7.7647058823529411,-2.263157894736842 -8.6315789473684212,-2.333333333333333 -8.6666666666666661))",
                                 "*********");
        test_geometry<mls, poly>("MULTILINESTRING((-2.333333333333333 -8.6666666666666661,-4.3253012048192767 -8.168674698795181,-4.1194968553459113 -7.6100628930817606,-2.8823529411764706 -7.7647058823529411,-2.882352941176471 -7.7647058823529411,-2.263157894736842 -8.6315789473684212,-2.333333333333333 -8.6666666666666661))",
                                 "POLYGON((-0.59322033898305082 -8.0508474576271194,-2.882352941176471 -7.7647058823529411,-2.8823529411764706 -7.7647058823529411,-3.7361111111111112 -6.5694444444444446,-3.4404145077720205 -5.766839378238342,-4.1864406779661012 -3.6779661016949152,-7.5252525252525251 -5.5858585858585865,-7.5862068965517242 -5.1896551724137936,-4.47887323943662 -2.859154929577465,-4.5789473684210531 -2.5789473684210527,-3 -1,-2.9310344827586206 -0.86206896551724144,-3.1764705882352944 -0.70588235294117663,-4.7401960784313726 -2.1274509803921577,-5.3255813953488369 -0.48837209302325502,-4.7872340425531918 0.31914893617021284,-5.8571428571428577 1.0000000000000007,-5.3255813953488369 -0.48837209302325502,-5.9473684210526319 -1.4210526315789465,-8 2,-7.7333333333333334 2.1939393939393939,-8.8294573643410867 2.891472868217055,-8.8556701030927822 3.061855670103093,-7.5999999999999996 3.6000000000000001,-7.7999999999999998 3.7999999999999998,-7.75 3.7916666666666665,-7.5471698113207548 3.6226415094339623,-7.3200000000000003 3.7200000000000002,-3.473684210526315 3.0789473684210527,-3.2549019607843133 3.2156862745098036,-2.9999999999999982 3.1428571428571423,-3.1733333333333325 3.2666666666666666,-2.9180327868852456 3.4262295081967209,-2.8723404255319145 3.1063829787234041,-2.1111111111111112 2.8888888888888888,-2.1428571428571428 2.8571428571428572,-1.8433734939759043 2.8072289156626509,-1.8396226415094346 2.8113207547169816,-1.6486486486486487 2.756756756756757,-1.76510067114094 2.8926174496644301,-0.53846153846153855 4.2307692307692308,1.8148148148148147 5.4074074074074074,1.588235294117647 2.2352941176470589,1.819672131147541 2.1967213114754101,2 4,2 2.1666666666666665,2.3538461538461544 2.1076923076923078,2 1.6875000000000004,2 -2,1.2173913043478262 -3.8260869565217392,1.7375886524822697 1.3758865248226959,1.5073170731707317 1.1024390243902444,1.1428571428571428 -4,-0.59322033898305082 -8.0508474576271194),(1.666666666666667 1.5999999999999988,1.5675675675675675 1.8378378378378377,1.4374999999999991 1.8750000000000002,1.0487804878048776 2.3414634146341466,0.46666666666666712 2.6060606060606055,0.086956521739131043 2.2608695652173911,1.4374999999999991 1.8750000000000002,1.666666666666667 1.5999999999999988))",
                                 "*********");
    }

    // 21.01.2015
    test_geometry<mls, poly>("MULTILINESTRING((6 6,15 15),(0 0, 7 7))",
                             "POLYGON((5 5,5 15,15 15,15 5,5 5))",
                             "101000212");
    test_geometry<mls, poly>("MULTILINESTRING((15 15,6 6),(0 0, 7 7))",
                             "POLYGON((5 5,5 15,15 15,15 5,5 5))",
                             "101000212");

    // extended
    test_geometry<mls, ring>("MULTILINESTRING((15 15,6 6),(4 14,6 15.986412732589057))",
                             "POLYGON((5 5,5 15,15 15,15 5,5 5))",
                             "101000212");

    // 23.01.2015
    test_geometry<mls, poly>("MULTILINESTRING((4 10.035925377760330, 3 10.031432746397092, 10 6),(5 0, 7 5, 9 10.013467818052765))",
                             "POLYGON((0 0,0 10,10 10,10 0,5 5,0 0))",
                             "111F00212");

    // 23.01.2015
    test_geometry<mls, poly>("MULTILINESTRING((3 10.031432746397092, 1 5, 1 10.013467818052765, 3 4, 7 8, 6 10.035925377760330, 10 2))",
                             "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                             "10FF0F212");
}

template <typename P>
void test_multi_linestring_multi_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::multi_polygon<poly> mpoly;

    // polygons with some ring equal to the linestrings
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10,0 10,0 0),(20 20,50 50,20 80,20 20))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                              "F11FFF2F2");

    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10,0 10,0 0),(2 2,5 5,2 8,2 2))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(2 2,5 5,2 8,2 2)))",
                              "F1FFFF2F2");


    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10),(10 10,0 10,0 0))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                              "F1FFFF2F2");
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10),(10 10,0 10,0 0),(20 20,50 50,20 80,20 20))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                              "F11FFF2F2");

    // disjoint
    test_geometry<mls, mpoly>("MULTILINESTRING((20 20,30 30),(30 30,40 40))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                              "FF1FF0212");

    test_geometry<mls, mpoly>("MULTILINESTRING((5 5,0 5),(5 5,5 0),(10 10,10 5,5 5,5 10,10 10))",
                              "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)),((5 5,10 1,10 0,5 5)))",
                              "F1FF0F212");
    test_geometry<mls, mpoly>("MULTILINESTRING((5 5,0 5),(5 5,5 0),(0 5,0 0,5 0),(10 10,10 5,5 5,5 10,10 10))",
                              "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)),((5 5,10 1,10 0,5 5)))",
                              "F1FFFF212");
    test_geometry<mls, mpoly>("MULTILINESTRING((5 5,0 0),(5 5,5 0),(10 10,10 5,5 5,5 10,10 10))",
                              "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)),((5 5,10 1,10 0,5 5)))",
                              "11FF0F212");

    // MySQL report 18.12.2014 - extended
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,4 -2,5 0),(5 -2,6 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              "10FFFF212");
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,0 1,5 0),(0 0,0 -1,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              "F01FFF212");
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,4 -2,5 0),(6 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              "10F0FF212");
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,0 1,5 0),(0 -1,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              "F01FF0212");
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,5 0),(5 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              "1010F0212");
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,5 0),(0 0,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              "1010F0212");

    // 22.01.2015 - extended
    test_geometry<mls, mpoly>("MULTILINESTRING((10 10,0 10),(5 5,0 0,10 0))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                              "11F00F212");
    test_geometry<mls, mpoly>("MULTILINESTRING((5 5,0 0,5 -5),(0 0,9 1))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                              "101000212");
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -5,0 0,5 5),(0 0,5 -1))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                              "101000212");
}

template <typename P>
void test_all()
{
    test_linestring_polygon<P>();
    test_linestring_multi_polygon<P>();
    test_multi_linestring_polygon<P>();
    test_multi_linestring_multi_polygon<P>();
}

int test_main( int , char* [] )
{
    typedef bg::cs::spherical_equatorial<bg::degree> cs_t;

    test_all<bg::model::point<float, 2, cs_t> >();
    test_all<bg::model::point<double, 2, cs_t> >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::point<ttmath_big, 2, cs_t> >();
#endif

    return 0;
}
