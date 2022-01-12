// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2015, 2016, 2017.
// Modifications copyright (c) 2015-2017, Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/geometry.hpp>
#include <geometry_test_common.hpp>

namespace bg = boost::geometry;

struct custom_karney
{
    template
    <
        typename CT,
        bool EnableDistance,
        bool EnableAzimuth,
        bool EnableReverseAzimuth = false,
        bool EnableReducedLength = false,
        bool EnableGeodesicScale = false
    >
    struct inverse
        : bg::formula::detail::karney_inverse
            <
                CT, EnableDistance,
                EnableAzimuth, EnableReverseAzimuth,
                EnableReducedLength, EnableGeodesicScale,
                4
            >
    {};
};

//Testing geographic strategies
template <typename CT>
void test_geo_strategies()
{
    std::string poly = "POLYGON((52 0, 41 -74, -23 -43, -26 28, 52 0))";

    typedef bg::model::point<CT, 2, bg::cs::geographic<bg::degree> > pt_geo;

    bg::strategy::area::geographic<> geographic_default;

    bg::strategy::area::geographic<bg::strategy::andoyer, 1>
        geographic_andoyer1;
    bg::strategy::area::geographic<bg::strategy::andoyer, 2>
        geographic_andoyer2;
    bg::strategy::area::geographic<bg::strategy::andoyer, 3>
        geographic_andoyer3;
    bg::strategy::area::geographic<bg::strategy::andoyer, 4>
        geographic_andoyer4;
    bg::strategy::area::geographic<bg::strategy::andoyer, 5>
        geographic_andoyer5;

    bg::strategy::area::geographic<bg::strategy::thomas, 1>
        geographic_thomas1;
    bg::strategy::area::geographic<bg::strategy::thomas, 2>
        geographic_thomas2;
    bg::strategy::area::geographic<bg::strategy::thomas, 3>
        geographic_thomas3;
    bg::strategy::area::geographic<bg::strategy::thomas, 4>
        geographic_thomas4;
    bg::strategy::area::geographic<bg::strategy::thomas, 5>
        geographic_thomas5;

    bg::strategy::area::geographic<bg::strategy::vincenty, 1>
        geographic_vincenty1;
    bg::strategy::area::geographic<bg::strategy::vincenty, 2>
        geographic_vincenty2;
    bg::strategy::area::geographic<bg::strategy::vincenty, 3>
        geographic_vincenty3;
    bg::strategy::area::geographic<bg::strategy::vincenty, 4>
        geographic_vincenty4;
    bg::strategy::area::geographic<bg::strategy::vincenty, 5>
        geographic_vincenty5;

    bg::strategy::area::geographic<bg::strategy::karney, 1>
        geographic_karney1;
    bg::strategy::area::geographic<bg::strategy::karney, 2>
        geographic_karney2;
    bg::strategy::area::geographic<bg::strategy::karney, 3>
        geographic_karney3;
    bg::strategy::area::geographic<bg::strategy::karney, 4>
        geographic_karney4;
    bg::strategy::area::geographic<bg::strategy::karney, 5>
        geographic_karney5;

    bg::strategy::area::geographic<custom_karney, 3>
        geographic_custom_karney3;

    bg::strategy::area::geographic<bg::strategy::andoyer>
        geographic_andoyer_default;
    bg::strategy::area::geographic<bg::strategy::thomas>
        geographic_thomas_default;
    bg::strategy::area::geographic<bg::strategy::vincenty>
        geographic_vincenty_default;

    bg::model::polygon<pt_geo> geometry_geo;

    //GeographicLib         63316536351834.289
    //PostGIS (v2.2.2)      6.33946+13
    //MS SQL SERVER         632930207487035

    bg::read_wkt(poly, geometry_geo);
    CT area;
    CT err = 0.0000001;

    CT area_default = bg::area(geometry_geo);
    BOOST_CHECK_CLOSE(area_default, 63316423532570.688, err);
    area = bg::area(geometry_geo, geographic_default);
    BOOST_CHECK_CLOSE(area, 63316423532570.688, err);

    CT area_less_accurate = bg::area(geometry_geo, geographic_andoyer1);
    BOOST_CHECK_CLOSE(area, 63316423532570.688, err);
    area = bg::area(geometry_geo, geographic_andoyer2);
    BOOST_CHECK_CLOSE(area, 63316423410597.016, err);
    area = bg::area(geometry_geo, geographic_andoyer3);
    BOOST_CHECK_CLOSE(area, 63316423410701.703, err);
    area = bg::area(geometry_geo, geographic_andoyer4);
    BOOST_CHECK_CLOSE(area, 63316423410701.602, err);
    area = bg::area(geometry_geo, geographic_andoyer5);
    BOOST_CHECK_CLOSE(area, 63316423410701.602, err);

    area = bg::area(geometry_geo, geographic_thomas1);
    BOOST_CHECK_CLOSE(area, 63316536214315.32, err);
    area = bg::area(geometry_geo, geographic_thomas2);
    BOOST_CHECK_CLOSE(area, 63316536092341.266, err);
    area = bg::area(geometry_geo, geographic_thomas3);
    BOOST_CHECK_CLOSE(area, 63316536092445.961, err);
    area = bg::area(geometry_geo, geographic_thomas4);
    BOOST_CHECK_CLOSE(area, 63316536092445.859, err);
    area = bg::area(geometry_geo, geographic_thomas5);
    BOOST_CHECK_CLOSE(area, 63316536092445.859, err);

    area = bg::area(geometry_geo, geographic_vincenty1);
    BOOST_CHECK_CLOSE(area, 63316536473798.984, err);
    area = bg::area(geometry_geo, geographic_vincenty2);
    BOOST_CHECK_CLOSE(area, 63316536351824.93, err);
    area = bg::area(geometry_geo, geographic_vincenty3);
    BOOST_CHECK_CLOSE(area, 63316536351929.625, err);
    area = bg::area(geometry_geo, geographic_vincenty4);
    BOOST_CHECK_CLOSE(area, 63316536351929.523, err);
    area = bg::area(geometry_geo, geographic_vincenty5);
    BOOST_CHECK_CLOSE(area, 63316536351929.523, err);

    area = bg::area(geometry_geo, geographic_karney1);
    BOOST_CHECK_CLOSE(area, 63316536473703.75, err);
    area = bg::area(geometry_geo, geographic_karney2);
    BOOST_CHECK_CLOSE(area, 63316536351729.695, err);
    area = bg::area(geometry_geo, geographic_karney3);
    BOOST_CHECK_CLOSE(area, 63316536351834.383, err);
    area = bg::area(geometry_geo, geographic_custom_karney3);
    BOOST_CHECK_CLOSE(area, 63316536351834.352, err);
    area = bg::area(geometry_geo, geographic_karney4);
    BOOST_CHECK_CLOSE(area, 63316536351834.281, err);
    CT area_most_accurate = bg::area(geometry_geo, geographic_karney5);
    BOOST_CHECK_CLOSE(area, 63316536351834.281, err);

    area = bg::area(geometry_geo, geographic_andoyer_default);
    BOOST_CHECK_CLOSE(area, 63316423532570.688, err);
    area = bg::area(geometry_geo, geographic_thomas_default);
    BOOST_CHECK_CLOSE(area, 63316536092341.266, err);
    area = bg::area(geometry_geo, geographic_vincenty_default);
    BOOST_CHECK_CLOSE(area, 63316536351929.523, err);

    BOOST_CHECK_CLOSE(area_most_accurate, area_less_accurate, .001);
    BOOST_CHECK_CLOSE(area_most_accurate, area_default, .001);

/*
    // timings and accuracy
    std::cout.precision(25);
    std::size_t exp_times = 100000;
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_andoyer1);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_andoyer2);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_andoyer3);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_andoyer4);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_andoyer5);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_thomas1);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_thomas2);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_thomas3);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_thomas4);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_thomas5);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_vincenty1);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_vincenty2);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_vincenty3);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_vincenty4);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
    {   clock_t startTime = clock();
        for (int j=0; j < exp_times; j++) area = bg::area(geometry_geo, geographic_vincenty5);
        std::cout << double( clock() - startTime ) / (double)CLOCKS_PER_SEC<< " ";
        std::cout  << area << std::endl;}
*/
}

int test_main(int, char* [])
{

    test_geo_strategies<double>();

    return 0;
}
