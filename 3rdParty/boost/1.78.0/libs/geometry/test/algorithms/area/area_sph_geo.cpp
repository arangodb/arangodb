// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2015-2021.
// Modifications copyright (c) 2015-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/geometry.hpp>
#include <geometry_test_common.hpp>

#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
#include <GeographicLib/Constants.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/PolygonArea.hpp>

template <typename polygon>
auto geographiclib_area(polygon p) {

    using namespace GeographicLib;

    //Geodesic geod(6371008.8, 0.0);//Constants::WGS84_f());
    const Geodesic& geod = Geodesic::WGS84();
    PolygonArea poly(geod);
    for (auto it = boost::begin(boost::geometry::exterior_ring(p));
        it != boost::end(boost::geometry::exterior_ring(p)); ++it)
    {
        double x = bg::get_as_radian<0>(*it) * bg::math::r2d<double>();
        double y = bg::get_as_radian<1>(*it) * bg::math::r2d<double>();
        poly.AddPoint( y,  x);
    }
    double perimeter, area;
    poly.Compute(false, true, perimeter, area);
    // geographiclib has different orientation than BG
    return -area;
}
#endif // BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB

namespace bg = boost::geometry;

//Testing spherical and geographic strategies
template <typename CT>
void test_spherical_geo()
{
    typedef CT ct;

    //Geographic

    typedef typename bg::model::point
        <
            ct, 2, bg::cs::geographic<bg::degree>
        > pt_geo;

    bg::strategy::area::geographic
        <
            bg::strategy::vincenty,
            5
        > area_geographic;

    bg::model::polygon<pt_geo> geometry_geo;

    //Spherical

    typedef typename bg::model::point
        <
            ct, 2, bg::cs::spherical_equatorial<bg::degree>
        > pt;
    bg::model::polygon<pt> geometry;

    // unit-sphere has area of 4-PI. Polygon covering 1/8 of it:
    std::string poly = "POLYGON((0 0,0 90,90 0,0 0))";

    bg::strategy::area::spherical
        <
            ct
        > strategy_unary(1.0);

    ct const four = 4.0;
    ct const eight = 8.0;
    ct expected = four * boost::geometry::math::pi<ct>() / eight;
    bg::read_wkt(poly, geometry);
    ct area = bg::area(geometry, strategy_unary);
    BOOST_CHECK_CLOSE(area, expected, 0.0001);

    // With strategy, radius 2 -> 4 pi r^2
    bg::strategy::area::spherical
        <
            ct
        > strategy(2.0);

    area = bg::area(geometry, strategy);
    ct const two = 2.0;
    BOOST_CHECK_CLOSE(area, two * two * expected, 0.0001);

    // Geographic total area of earth is about 510065626583900.6 (WGS84 ellipsoid)
    // (510072000 in https://en.wikipedia.org/wiki/Earth)
    // So the 1/8 is 6.375820332*10^13 and here we get something close to it
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    //GeoGraphicLib gives: 63758202715511.055
    BOOST_CHECK_CLOSE(area, 63758202715509.844, 0.0001);


    // Wrangel Island (dateline crossing)
    // With (spherical) Earth strategy
    poly = "POLYGON((-178.7858 70.7852, 177.4758 71.2333, 179.7436 71.5733, -178.7858 70.7852))";

    bg::strategy::area::spherical
        <
            ct
        > spherical_earth(6373);
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    // SQL Server gives: 4537.9654419375
    // PostGIS gives: 4537.9311668307
    // Note: those are Geographic, this test is Spherical
    BOOST_CHECK_CLOSE(area, 4506.6389, 0.001);

    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 4537929936.5883484, 0.0001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area, 0.1);
#endif
    // Wrangel, more in detail
    poly = "POLYGON((-178.568604 71.564148,-178.017548 71.449692,-177.833313 71.3461,\
            -177.502838 71.277466 ,-177.439453 71.226929,-177.620026 71.116638,\
            -177.9389 71.037491,-178.8186 70.979965,-179.274445 70.907761,\
            -180 70.9972,179.678314 70.895538,179.272766 70.888596,\
             178.791016 70.7964,178.617737 71.035538,178.872192 71.217484,\
             179.530273 71.4383 ,-180 71.535843 ,-179.628601 71.577194,\
            -179.305298 71.551361,-179.03421 71.597748,-178.568604 71.564148))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    // SQL Server gives: 7669.10402181435
    // PostGIS gives: 7669.55565459832
    BOOST_CHECK_CLOSE(area, 7616.523769, 0.001);

    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 7669498457.4130802, 0.0001);

    // Check more at the equator
    /*
    select 1,geography::STGeomFromText('POLYGON((-178.7858 10.7852 , 179.7436 11.5733 , \
                        177.4758 11.2333 , -178.7858 10.7852))',4326) .STArea()/1000000.0
    union select 2,geography::STGeomFromText('POLYGON((-178.7858 20.7852 , 179.7436 21.5733 ,\
                        177.4758 21.2333 , -178.7858 20.7852))',4326) .STArea()/1000000.0
    union select 3,geography::STGeomFromText('POLYGON((-178.7858 30.7852 , 179.7436 31.5733 , \
                        177.4758 31.2333 , -178.7858 30.7852))',4326) .STArea()/1000000.0
    union select 0,geography::STGeomFromText('POLYGON((-178.7858 0.7852 , 179.7436 1.5733 , \
                        177.4758 1.2333 , -178.7858 0.7852))',4326) .STArea()/1000000.0
    union select 4,geography::STGeomFromText('POLYGON((-178.7858 40.7852 , 179.7436 41.5733 , \
                        177.4758 41.2333 , -178.7858 40.7852))',4326) .STArea()/1000000.0
    */

    poly = "POLYGON((-178.7858 0.7852, 177.4758 1.2333, 179.7436 1.5733, -178.7858 0.7852))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 14136.09946, 0.001); // SQL Server gives: 14064.1902284513
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 14064129044.677765, 0.0001);

    poly = "POLYGON((-178.7858 10.7852, 177.4758 11.2333, 179.7436 11.5733, -178.7858 10.7852))";
    // Geographiclib 1.51 :13696308940.35794067382812
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 13760.2456, 0.001); // SQL Server gives: 13697.0941155193
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 13696308940.342197, 0.0001);

    poly = "POLYGON((-178.7858 20.7852, 177.4758 21.2333, 179.7436 21.5733, -178.7858 20.7852))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 12987.8682, 0.001); // SQL Server gives: 12944.3970990317 -> -39m^2
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 12943176284.489822, 0.0001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area, 0.00001);
#endif

    poly = "POLYGON((-178.7858 30.7852, 177.4758 31.2333, 179.7436 31.5733, -178.7858 30.7852))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 11856.3935, 0.001); // SQL Server gives: 11838.5338423574 -> -18m^2
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 11837280445.394035, 0.0001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    // GEOGRAPHICLIB 1.51: 11837280466.204895
    BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area, 0.000001);
#endif

    poly = "POLYGON((-178.7858 40.7852, 177.4758 41.2333, 179.7436 41.5733, -178.7858 40.7852))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 10404.627685523914, 0.001);
    // SQL Server gives: 10412.0607137119, -> +8m^2
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 10411098789.387386, 0.0001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    // GEOGRAPHICLIB 1.51: 10411098790.577026
    BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area, 0.000001);
#endif

    // Concave
    poly = "POLYGON((0 40,1 42,0 44,2 43,4 44,3 42,4 40,2 41,0 40))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 73538.2958, 0.001); // SQL Server gives: 73604.2047689719
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 73604163095.545319, 0.0001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    // GEOGRAPHICLIB 1.51: 73604163091.274048
    BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area, 0.000001);
#endif

    // With hole POLYGON((0 40,4 40,4 44,0 44,0 40),(1 41,2 43,3 42,1 41))
    poly = "POLYGON((0 40,0 44,4 44,4 40,0 40),(1 41,3 42,2 43,1 41))";
    bg::read_wkt(poly, geometry);
    area = bg::area(geometry, spherical_earth);
    BOOST_CHECK_CLOSE(area, 133233.844876, 0.001); // SQL Server gives: 133353.335
    bg::read_wkt(poly, geometry_geo);
    area = bg::area(geometry_geo, area_geographic);
    BOOST_CHECK_CLOSE(area, 133353077343.26692, 0.0001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    // GEOGRAPHICLIB 1.51: 147189206350.20142
    //BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area, 0.9);
#endif

    // mean Earth's radius^2
    double r2 = bg::math::sqr(bg::get_radius<0>(bg::srs::sphere<double>()));

    // around 0 meridian
    {
        std::string poly1 = "POLYGON((-10 0,-10 10,0 10,0 0,-10 0))";
        std::string poly2 = "POLYGON((0 0,0 10,10 10,10 0,0 0))";
        std::string poly3 = "POLYGON((-5 0,-5 10,5 10,5 0,-5 0))";
        bg::read_wkt(poly1, geometry);
        ct area1 = bg::area(geometry);
        bg::read_wkt(poly2, geometry);
        ct area2 = bg::area(geometry);
        bg::read_wkt(poly3, geometry);
        ct area3 = bg::area(geometry);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area1 * r2, 1233204227903.1848, 0.001);
        //geographic
        bg::read_wkt(poly1, geometry_geo);
        area1 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly2, geometry_geo);
        area2 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly3, geometry_geo);
        area3 = bg::area(geometry_geo, area_geographic);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area1, 1227877191611.3574, 0.001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
        // GEOGRAPHICLIB 1.51: 1227877191609.6267
        BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area3, 0.000001);
#endif
    }
    {
        std::string poly1 = "POLYGON((-10 -5,-10 5,0 5,0 -5,-10 -5))";
        std::string poly2 = "POLYGON((0 -5,0 5,10 5,10 -5,0 -5))";
        std::string poly3 = "POLYGON((-5 -5,-5 5,5 5,5 -5,-5 -5))";
        bg::read_wkt(poly1, geometry);
        ct area1 = bg::area(geometry);
        bg::read_wkt(poly2, geometry);
        ct area2 = bg::area(geometry);
        bg::read_wkt(poly3, geometry);
        ct area3 = bg::area(geometry);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area1 * r2, 1237986107636.0261, 0.001);
        //geographic
        bg::read_wkt(poly1, geometry_geo);
        area1 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly2, geometry_geo);
        area2 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly3, geometry_geo);
        area3 = bg::area(geometry_geo, area_geographic);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area1, 1232514639151.7168, 0.001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
        // GEOGRAPHICLIB 1.51: 1232514639151.6357
        BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area3, 0.000001);
#endif
    }
    // around 180 meridian
    {
        std::string poly1 = "POLYGON((-180 0,-180 10,-170 10,-170 0,-180 0))";
        std::string poly2 = "POLYGON((175 0,175 10,-175 10,-175 0,175 0))";
        std::string poly3 = "POLYGON((170 0,170 10,180 10,180 0,170 0))";
        std::string poly4 = "POLYGON((170 0,170 10,-180 10,-180 0,170 0))";
        std::string poly5 = "POLYGON((180 0,180 10,-170 10,-170 0,180 0))";
        bg::read_wkt(poly1, geometry);
        ct area1 = bg::area(geometry);
        bg::read_wkt(poly2, geometry);
        ct area2 = bg::area(geometry);
        bg::read_wkt(poly3, geometry);
        ct area3 = bg::area(geometry);
        bg::read_wkt(poly4, geometry);
        ct area4 = bg::area(geometry);
        bg::read_wkt(poly5, geometry);
        ct area5 = bg::area(geometry);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area3, area4, 0.001);
        BOOST_CHECK_CLOSE(area4, area5, 0.001);
        BOOST_CHECK_CLOSE(area1 * r2, 1233204227903.1833, 0.001);
        //geographic
        bg::read_wkt(poly1, geometry_geo);
        area1 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly2, geometry_geo);
        area2 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly3, geometry_geo);
        area3 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly4, geometry_geo);
        area4 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly5, geometry_geo);
        area5 = bg::area(geometry_geo, area_geographic);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area3, area4, 0.001);
        BOOST_CHECK_CLOSE(area4, area5, 0.001);
        BOOST_CHECK_CLOSE(area1, 1227877191611.3574, 0.001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
        // GEOGRAPHICLIB 1.51: 1227877191609.6267
        BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area5, 0.000001);
#endif
    }
    {
        std::string poly1 = "POLYGON((-180 -5,-180 5,-170 5,-170 -5,-180 -5))";
        std::string poly2 = "POLYGON((175 -5,175 5,-175 5,-175 -5,175 -5))";
        std::string poly3 = "POLYGON((170 -5,170 5,180 5,180 -5,170 -5))";
        std::string poly4 = "POLYGON((170 -5,170 5,180 5,180 -5,170 -5))";
        std::string poly5 = "POLYGON((180 -5,180 5,-170 5,-170 -5,180 -5))";
        bg::read_wkt(poly1, geometry);
        ct area1 = bg::area(geometry);
        bg::read_wkt(poly2, geometry);
        ct area2 = bg::area(geometry);
        bg::read_wkt(poly3, geometry);
        ct area3 = bg::area(geometry);
        bg::read_wkt(poly4, geometry);
        ct area4 = bg::area(geometry);
        bg::read_wkt(poly5, geometry);
        ct area5 = bg::area(geometry);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area3, area4, 0.001);
        BOOST_CHECK_CLOSE(area4, area5, 0.001);
        BOOST_CHECK_CLOSE(area1 * r2, 1237986107636.0247, 0.001);
        //geographic
        bg::read_wkt(poly1, geometry_geo);
        area1 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly2, geometry_geo);
        area2 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly3, geometry_geo);
        area3 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly4, geometry_geo);
        area4 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly5, geometry_geo);
        area5 = bg::area(geometry_geo, area_geographic);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area3, area4, 0.001);
        BOOST_CHECK_CLOSE(area4, area5, 0.001);
        BOOST_CHECK_CLOSE(area1, 1232514639151.7168, 0.001);
#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
        // GEOGRAPHICLIB 1.51: 1232514639151.6357
        BOOST_CHECK_CLOSE(geographiclib_area(geometry_geo), area5, 0.000001);
#endif
    }
    // around poles
    {
        std::string poly1 = "POLYGON((0 80,-90 80,-180 80,90 80,0 80))";
        std::string poly2 = "POLYGON((0 80,-90 80,180 80,90 80,0 80))";
        std::string poly3 = "POLYGON((0 -80,90 -80,-180 -80,-90 -80,0 -80))";
        std::string poly4 = "POLYGON((0 -80,90 -80,180 -80,-90 -80,0 -80))";
        bg::read_wkt(poly1, geometry);
        ct area1 = bg::area(geometry);
        bg::read_wkt(poly2, geometry);
        ct area2 = bg::area(geometry);
        bg::read_wkt(poly3, geometry);
        ct area3 = bg::area(geometry);
        bg::read_wkt(poly4, geometry);
        ct area4 = bg::area(geometry);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area3, area4, 0.001);
        //geographic
        bg::read_wkt(poly1, geometry_geo);
        area1 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly2, geometry_geo);
        area2 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly3, geometry_geo);
        area3 = bg::area(geometry_geo, area_geographic);
        bg::read_wkt(poly4, geometry_geo);
        area4 = bg::area(geometry_geo, area_geographic);
        BOOST_CHECK_CLOSE(area1, area2, 0.001);
        BOOST_CHECK_CLOSE(area2, area3, 0.001);
        BOOST_CHECK_CLOSE(area3, area4, 0.001);
    }

    {
        bg::model::ring<pt> aurha; // a'dam-utr-rott.-den haag-a'dam
        std::string poly = "POLYGON((4.892 52.373,5.119 52.093,4.479 51.930,\
                                     4.23 52.08,4.892 52.373))";
        bg::read_wkt(poly, aurha);
        /*if (polar)
        {
            // Create colatitudes (measured from pole)
            for (pt& p : aurha)
            {
                bg::set<1>(p, ct(90) - bg::get<1>(p));
            }
            bg::correct(aurha);
        }*/
        bg::strategy::area::spherical
            <
                ct
            > area_spherical(6372.795);
        area = bg::area(aurha, area_spherical);
        BOOST_CHECK_CLOSE(area, 1476.645675, 0.0001);
        //geographic
        bg::read_wkt(poly, geometry_geo);
        area = bg::area(geometry_geo, area_geographic);
        BOOST_CHECK_CLOSE(area, 1481555970.0765088, 0.001);

        // SQL Server gives: 1481.55595960659
        // for select geography::STGeomFromText('POLYGON((4.892 52.373,4.23 52.08,
        //      4.479 51.930,5.119 52.093,4.892 52.373))',4326).STArea()/1000000.0
    }

    {
        bg::model::polygon<pt, false> geometry_sph;
        std::string wkt = "POLYGON((0 0, 5 0, 5 5, 0 5, 0 0))";
        bg::read_wkt(wkt, geometry_sph);

        area = bg::area(geometry_sph, bg::strategy::area::spherical<>(6371228.0));
        BOOST_CHECK_CLOSE(area, 308932296103.83051, 0.0001);

        bg::model::polygon<pt_geo, false> geometry_geo;
        bg::read_wkt(wkt, geometry_geo);

        area = bg::area(geometry_geo, bg::strategy::area::geographic<>(bg::srs::spheroid<double>(6371228.0, 6371228.0)));
        BOOST_CHECK_CLOSE(area, 308932296103.82574, 0.001);
    }

    {
        // small areas: ~20m^2
        // see https://github.com/boostorg/geometry/issues/799
        // geographiclib 1.50.1 returns: 25.5736

        bg::model::polygon<pt> geometry;
        std::string wkt ="POLYGON((-0.020000 51.470027,-0.020031 51.470019,-0.020043 51.470000,\
                 -0.020031 51.469981,-0.020000 51.469973,-0.019969 51.469981,\
                 -0.019957 51.470000,-0.019969 51.470019,-0.020000 51.470027))";
        bg::read_wkt(wkt, geometry);

        bg::strategy::area::spherical
            <
                ct
            > area_spherical(6371228.0);
        area = bg::area(geometry, area_spherical);
        BOOST_CHECK_CLOSE(area, -25.48014, 0.0001);

        //geographic

        using pt_geo_d = bg::model::point
            <
                double, 2, bg::cs::geographic<bg::degree>
            >;
        using pt_geo_ld = bg::model::point
            <
                long double, 2, bg::cs::geographic<bg::degree>
            >;

        bg::strategy::area::geographic<bg::strategy::andoyer> area_a;
        bg::strategy::area::geographic<bg::strategy::thomas> area_t;
        bg::strategy::area::geographic<bg::strategy::vincenty> area_v;
        bg::strategy::area::geographic<bg::strategy::karney> area_k;

        bg::model::polygon<pt_geo_d> geometry_geo_d;

        bg::read_wkt(wkt, geometry_geo_d);
        area = bg::area(geometry_geo_d, area_a);
        BOOST_CHECK_CLOSE(area, -25.47837, 0.001);
        area = bg::area(geometry_geo_d, area_t);
        BOOST_CHECK_CLOSE(area, -25.57355, 0.001);
        area = bg::area(geometry_geo_d, area_v);
        BOOST_CHECK_CLOSE(area, -25.55881, 0.001);
        area = bg::area(geometry_geo_d, area_k);
        BOOST_CHECK_CLOSE(area, -25.57357, 0.001);

        bg::model::polygon<pt_geo_ld> geometry_geo_ld;

        bg::read_wkt(wkt, geometry_geo_ld);
        area = bg::area(geometry_geo_ld, area_a);
        BOOST_CHECK_CLOSE(area, -25.57978, 0.4); // -25.478374 with vc-14.1
        area = bg::area(geometry_geo_ld, area_t);
        BOOST_CHECK_CLOSE(area, -25.57359, 0.001);
        area = bg::area(geometry_geo_ld, area_v);
        BOOST_CHECK_CLOSE(area, -25.57394, 0.06); // -25.558816 with vc-14.1
        area = bg::area(geometry_geo_ld, area_k);
        BOOST_CHECK_CLOSE(area, -25.57359, 0.001);
    }

    {
        // very small areas: ~2m^2
        // geographiclib 1.50.1 returns: 2.24581

        bg::model::polygon<pt> geometry;
        std::string wkt ="POLYGON((0.020000 51.470027,0.020005 51.470027,\
            0.020005 51.470000,0.020007 51.4699,0.020000 51.470027))";
        bg::read_wkt(wkt, geometry);

        bg::strategy::area::spherical
            <
                ct
            > area_spherical(6371228.0);
        area = bg::area(geometry, area_spherical);
        BOOST_CHECK_CLOSE(area, 2.2375981058307093, 0.001);

        //geographic

        using pt_geo_d = bg::model::point
            <
                double, 2, bg::cs::geographic<bg::degree>
            >;
        using pt_geo_ld = bg::model::point
            <
                long double, 2, bg::cs::geographic<bg::degree>
            >;

        bg::strategy::area::geographic<bg::strategy::andoyer> area_a;
        bg::strategy::area::geographic<bg::strategy::thomas> area_t;
        bg::strategy::area::geographic<bg::strategy::vincenty> area_v;
        bg::strategy::area::geographic<bg::strategy::karney> area_k;

        bg::model::polygon<pt_geo_d> geometry_geo_d;

        bg::read_wkt(wkt, geometry_geo_d);
        area = bg::area(geometry_geo_d, area_a);
        BOOST_CHECK_CLOSE(area, 2.2374430036130444, 0.001);
        area = bg::area(geometry_geo_d, area_t);
        BOOST_CHECK_CLOSE(area, 2.245814319435655, 0.001);
        area = bg::area(geometry_geo_d, area_v);
        BOOST_CHECK_CLOSE(area, 2.2374430036130444, 0.001);
        area = bg::area(geometry_geo_d, area_k);
        BOOST_CHECK_CLOSE(area, 2.2458140167194878, 0.001);

        bg::model::polygon<pt_geo_ld> geometry_geo_ld;

        bg::read_wkt(wkt, geometry_geo_ld);
        area = bg::area(geometry_geo_ld, area_a);
        BOOST_CHECK_CLOSE(area, 2.2374430039839437, 0.001);
        area = bg::area(geometry_geo_ld, area_t);
        BOOST_CHECK_CLOSE(area, 2.2457934740573235, 0.001);
        area = bg::area(geometry_geo_ld, area_v);
        BOOST_CHECK_CLOSE(area, 2.2374430039839437, 0.001);
        area = bg::area(geometry_geo_ld, area_k);
        BOOST_CHECK_CLOSE(area, 2.2458050314935392, 0.001);
    }

}

int test_main(int, char* [])
{

    test_spherical_geo<double>();

    return 0;
}
