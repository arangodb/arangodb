// Boost.Geometry
// Unit Test

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/srs/transformation.hpp>

#include "check_geometry.hpp"

//#include <proj_api.h>

template <typename T>
void test_geometries()
{
    using namespace boost::geometry;
    using namespace boost::geometry::model;
    using namespace boost::geometry::srs;

    typedef model::point<T, 2, cs::cartesian> point;
    typedef model::segment<point> segment;
    typedef model::linestring<point> linestring;
    typedef model::ring<point> ring;
    typedef model::polygon<point> polygon;
    typedef model::multi_point<point> mpoint;
    typedef model::multi_linestring<linestring> mlinestring;
    typedef model::multi_polygon<polygon> mpolygon;

    std::cout << std::setprecision(12);

    double d2r = math::d2r<T>();
    //double r2d = math::r2d<T>();

    std::string from = "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs";
    std::string to = "+proj=longlat +ellps=airy +datum=OSGB36 +no_defs";

    {
        point pt(18.5 * d2r, 54.2 * d2r);
        point pt2(0, 0);
        segment seg(pt, pt);
        segment seg2;
        linestring ls; ls.push_back(pt);
        linestring ls2;
        ring rg; rg.push_back(pt);
        ring rg2;
        polygon poly; poly.outer() = rg;
        polygon poly2;
        mpoint mpt; mpt.push_back(pt);
        mpoint mpt2;
        mlinestring mls; mls.push_back(ls);
        mlinestring mls2;
        mpolygon mpoly; mpoly.push_back(poly);
        mpolygon mpoly2;

        transformation<> tr((proj4(from)), (proj4(to)));
        //transformation<> tr((epsg(4326)), (epsg(25832)));       

        tr.forward(pt, pt2);
        tr.forward(seg, seg2);
        tr.forward(ls, ls2);
        tr.forward(rg, rg2);
        tr.forward(poly, poly2);
        tr.forward(mpt, mpt2);
        tr.forward(mls, mls2);
        tr.forward(mpoly, mpoly2);

        test::check_geometry(pt2, "POINT(0.322952937968 0.9459567165)", 0.001);
        test::check_geometry(seg2, "LINESTRING(0.322952937968 0.9459567165,0.322952937968 0.9459567165)", 0.001);
        test::check_geometry(ls2, "LINESTRING(0.322952937968 0.9459567165)", 0.001);
        test::check_geometry(rg2, "POLYGON((0.322952937968 0.9459567165))", 0.001);
        test::check_geometry(poly2, "POLYGON((0.322952937968 0.9459567165))", 0.001);
        test::check_geometry(mpt2, "MULTIPOINT((0.322952937968 0.9459567165))", 0.001);
        test::check_geometry(mls2, "MULTILINESTRING((0.322952937968 0.9459567165))", 0.001);
        test::check_geometry(mpoly2, "MULTIPOLYGON(((0.322952937968 0.9459567165)))", 0.001);
        
        tr.inverse(pt2, pt);
        tr.inverse(seg2, seg);
        tr.inverse(ls2, ls);
        tr.inverse(rg2, rg);
        tr.inverse(poly2, poly);
        tr.inverse(mpt2, mpt);
        tr.inverse(mls2, mls);
        tr.inverse(mpoly2, mpoly);
        
        test::check_geometry(pt, "POINT(0.322885911738 0.945968454552)", 0.001);
        test::check_geometry(seg, "LINESTRING(0.322885911738 0.945968454552,0.322885911738 0.945968454552)", 0.001);
        test::check_geometry(ls, "LINESTRING(0.322885911738 0.945968454552)", 0.001);
        test::check_geometry(rg, "POLYGON((0.322885911738 0.945968454552))", 0.001);
        test::check_geometry(poly, "POLYGON((0.322885911738 0.945968454552))", 0.001);
        test::check_geometry(mpt, "MULTIPOINT((0.322885911738 0.945968454552))", 0.001);
        test::check_geometry(mls, "MULTILINESTRING((0.322885911738 0.945968454552))", 0.001);
        test::check_geometry(mpoly, "MULTIPOLYGON(((0.322885911738 0.945968454552)))", 0.001);
    }

    /*{
        projPJ pj_from, pj_to;

        pj_from = pj_init_plus(from.c_str());
        pj_to = pj_init_plus(to.c_str());
        
        double x = get<0>(pt_xy);
        double y = get<1>(pt_xy);
        pj_transform(pj_from, pj_to, 1, 0, &x, &y, NULL );

        std::cout << x * r2d << " " << y * r2d << std::endl;
    }*/
}

template <typename P1, typename P2, typename Tr>
inline void test_combination(Tr const& tr, P1 const& pt,
                             std::string const& expected_fwd,
                             P1 const& expected_inv)
{
    using namespace boost::geometry;

    P2 pt2;

    tr.forward(pt, pt2);

    test::check_geometry(pt2, expected_fwd, 0.001);

    P1 pt1;

    tr.inverse(pt2, pt1);

    test::check_geometry(pt1, expected_inv, 0.001);
}

void test_combinations(std::string const& from, std::string const& to,
                       std::string const& in_deg,
                       std::string const& expected_deg,
                       std::string const& expected_rad,
                       std::string const& expected_inv_deg)
{
    using namespace boost::geometry;
    using namespace boost::geometry::model;
    using namespace boost::geometry::srs;

    typedef model::point<double, 2, cs::cartesian> xy;
    typedef model::point<double, 2, cs::geographic<degree> > ll_d;
    typedef model::point<double, 2, cs::geographic<radian> > ll_r;
    //typedef model::point<double, 3, cs::cartesian> xyz;
    //typedef model::point<double, 3, cs::geographic<degree> > llz_d;
    //typedef model::point<double, 3, cs::geographic<radian> > llz_r;

    transformation<> tr((proj4(from)), (proj4(to)));

    ll_d d;
    bg::read_wkt(in_deg, d);
    ll_r r(bg::get_as_radian<0>(d), bg::get_as_radian<1>(d));
    xy c(bg::get<0>(r), bg::get<1>(r));

    ll_d inv_d;
    bg::read_wkt(expected_inv_deg, inv_d);
    ll_r inv_r(bg::get_as_radian<0>(inv_d), bg::get_as_radian<1>(inv_d));
    xy inv_c(bg::get<0>(inv_r), bg::get<1>(inv_r));

    test_combination<xy, xy>(tr, c, expected_rad, inv_c);
    test_combination<xy, ll_r>(tr, c, expected_rad, inv_c);
    test_combination<xy, ll_d>(tr, c, expected_deg, inv_c);
    test_combination<ll_r, xy>(tr, r, expected_rad, inv_r);
    test_combination<ll_r, ll_r>(tr, r, expected_rad, inv_r);
    test_combination<ll_r, ll_d>(tr, r, expected_deg, inv_r);
    test_combination<ll_d, xy>(tr, d, expected_rad, inv_d);
    test_combination<ll_d, ll_r>(tr, d, expected_rad, inv_d);
    test_combination<ll_d, ll_d>(tr, d, expected_deg, inv_d);
}

int test_main(int, char*[])
{
    test_geometries<double>();
    test_geometries<float>();
    
    test_combinations("+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs",
                      "+proj=longlat +ellps=airy +datum=OSGB36 +no_defs",
                      "POINT(18.5 54.2)",
                      "POINT(18.5038403269 54.1993274575)",
                      "POINT(0.322952937968 0.9459567165)",
                      "POINT(18.5 54.2)");
    test_combinations("+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs",
                      "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +no_defs",
                      "POINT(18.5 54.2)",
                      "POINT(2059410.57968 7208125.2609)",
                      "POINT(2059410.57968 7208125.2609)",
                      "POINT(18.5 54.2)");
    test_combinations("+proj=longlat +ellps=clrk80 +units=m +no_defs",
                      "+proj=tmerc +lat_0=0 +lon_0=-62 +k=0.9995000000000001 +x_0=400000 +y_0=0 +ellps=clrk80 +units=m +no_defs",
                      "POINT(1 1)",
                      "POINT(9413505.3284665551 237337.74515944949)",
                      "POINT(9413505.3284665551 237337.74515944949)",
                      // this result seems to be wrong, it's the same with projection
                      "POINT(-2.4463131191981073 1.5066638962045082)");
    
    return 0;
}
