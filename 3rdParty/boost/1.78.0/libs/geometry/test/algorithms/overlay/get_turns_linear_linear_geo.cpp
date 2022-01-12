// Boost.Geometry
// Unit Test

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_get_turns.hpp"
#include <boost/geometry/geometries/geometries.hpp>


template <typename T>
void test_radian()
{
    typedef bg::model::point<T, 2, bg::cs::geographic<bg::radian> > pt;
    typedef bg::model::linestring<pt> ls;
    typedef bg::model::multi_linestring<ls> mls;

    bg::srs::spheroid<double> sph_wgs84(6378137.0, 6356752.3142451793);
    boost::geometry::strategies::relate::geographic<> wgs84(sph_wgs84);

    test_geometry<ls, mls>(
        "LINESTRING(0 0, -3.14159265358979 0)",
        "MULTILINESTRING((-2.1467549799530232 -0.12217304763960295,"
                         "-2.5481807079117185 -0.90757121103705041,"
                         "-2.6529004630313784 0.85521133347722067,"
                         " 0.92502450355699373 0.62831853071795796,"
                         "-2.5307274153917754 0,"
                         " 2.8099800957108676 1.0646508437165401,"
                         "-1.6057029118347816 -1.5009831567151219,"
                         " 0.2268928027592626 1.0646508437165401,"
                         "-2.199114857512853 -0.017453292519943278,"
                         " 0 0.31415926535897898,"
                         " 0 0.57595865315812822,"
                         " 1.0471975511965967 -0.73303828583761765,"
                         " 2.1118483949131366 -0.54105206811824158))",
                         expected("mii++")("muu==")("iuu++")("iuu++")("iuu++")("iuu++"),
                         wgs84);
}

int test_main(int, char* [])
{
    test_radian<double>();

    return 0;
}
