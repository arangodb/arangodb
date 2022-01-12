// Boost.Geometry (aka GGL, Generic Geometry Library)
// (Unit) Test

// Copyright (c) 2018 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <iomanip>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/simplify.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>

#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif


template <typename MultiPolygon>
std::string read_from_file(std::string const& filename)
{
    MultiPolygon mp;
    std::ifstream in(filename.c_str());
    while (in.good())
    {
        std::string line;
        std::getline(in, line);
        if (! line.empty())
        {
            typename boost::range_value<MultiPolygon>::type pol;
            bg::read_wkt(line, pol);
            mp.push_back(pol);
        }
    }
    std::ostringstream out;
    if (! mp.empty())
    {
        out << std::fixed << std::setprecision(19) << bg::wkt(mp);
    }

    BOOST_CHECK(! out.str().empty());

    return out.str();
}


template <typename MultiPolygon>
void test_one(std::string const& caseid, std::string const& wkt,
              double distance_in_meters,
              double expected_area_ratio, double expected_perimeter_ratio,
              std::size_t expected_polygon_count = 0,
              std::size_t expected_interior_count = 0,
              std::size_t expected_point_count = 0)
{
    boost::ignore_unused(caseid);

    MultiPolygon geometry, simplified;
    bg::read_wkt(wkt, geometry);
    bg::correct(geometry);
    bg::simplify(geometry, simplified, distance_in_meters);

    double const area_ratio = bg::area(simplified) / bg::area(geometry);
    double const perimeter_ratio = bg::perimeter(simplified) / bg::perimeter(geometry);

    BOOST_CHECK_CLOSE(perimeter_ratio, expected_perimeter_ratio, 0.01);
    BOOST_CHECK_CLOSE(area_ratio, expected_area_ratio, 0.01);
    BOOST_CHECK_EQUAL(expected_polygon_count, boost::size(simplified));
    BOOST_CHECK_EQUAL(expected_interior_count, bg::num_interior_rings(simplified));
    BOOST_CHECK_EQUAL(expected_point_count, bg::num_points(simplified));

//  To add new tests, this is convenient and write the test itself:
//    std::cout << "test_one<mp>(\"" << caseid << "\", " << caseid
//              << ", " << distance_in_meters
//              << std::setprecision(6)
//              << ", " << area_ratio
//              << ", " << perimeter_ratio
//              << ", " << boost::size(simplified)
//              << ", " << bg::num_interior_rings(simplified)
//              << ", " << bg::num_points(simplified)
//              << ");"
//              << std::endl;

#if defined(TEST_WITH_SVG)
    {
        typedef typename boost::range_value<MultiPolygon>::type polygon;

        std::ostringstream filename;
        filename << "simplify_" << caseid << "_" << distance_in_meters << ".svg";

        std::ofstream svg(filename.str().c_str());

        bg::svg_mapper
            <
                typename bg::point_type<MultiPolygon>::type
            > mapper(svg, 1200, 800);
        mapper.add(geometry);
        mapper.add(simplified);

        mapper.map(geometry, "fill-opacity:0.5;fill:rgb(153,204,0);"
                "stroke:rgb(153,204,0);stroke-width:1");
        for (polygon const& pol : simplified)
        {
            mapper.map(pol,
                       bg::area(pol) > 0 ? "fill:none;stroke:rgb(255,0,0);stroke-width:1"
                       : "fill:none;stroke:rgb(255,0,255);stroke-width:1");
        }
    }
#endif
}


template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise> polygon;
    typedef bg::model::multi_polygon<polygon> mp;

    // The unit test uses countries originally added for buffer unit test
    std::string base_folder = "buffer/data/";

    // Verify for Greece, Italy, Netherlands, Norway and UK
    std::string gr = read_from_file<mp>(base_folder + "gr.wkt");
    std::string it = read_from_file<mp>(base_folder + "it.wkt");
    std::string nl = read_from_file<mp>(base_folder + "nl.wkt");
    std::string no = read_from_file<mp>(base_folder + "no.wkt");
    std::string uk = read_from_file<mp>(base_folder + "uk.wkt");

    // Gradually simplify more aggresively.
    // Area ratio (first) can increase or decrease
    // Perimeter ratio (second) should decrease.
    // Polygons, interior rings, points should decrease
    test_one<mp>("gr", gr, 100, 0.999905, 0.999758, 68, 0, 2520);
    test_one<mp>("gr", gr, 200, 0.999773, 0.998865, 68, 0, 2019);
    test_one<mp>("gr", gr, 500, 0.999026, 0.995931, 68, 0, 1468);
    test_one<mp>("gr", gr, 1000, 0.997782, 0.991475, 68, 0, 1132);
    test_one<mp>("gr", gr, 2000, 0.994448, 0.9793, 65, 0, 854);
    test_one<mp>("gr", gr, 5000, 0.979743, 0.910266, 50, 0, 471);
    test_one<mp>("gr", gr, 10000, 0.968349, 0.778863, 28, 0, 245);
    test_one<mp>("gr", gr, 20000, 0.961943, 0.607009, 10, 0, 97); // Many islands disappear

    test_one<mp>("it", it, 100, 1.00001, 0.999813, 22, 1, 1783);
    test_one<mp>("it", it, 200, 1.00009, 0.9991, 22, 1, 1406);
    test_one<mp>("it", it, 500, 1.00019, 0.996848, 22, 1, 1011);
    test_one<mp>("it", it, 1000, 1.00041, 0.99294, 22, 1, 749);
    test_one<mp>("it", it, 2000, 1.00086, 0.985144, 22, 1, 546);
    test_one<mp>("it", it, 5000, 1.00147, 0.93927, 11, 1, 283);
    test_one<mp>("it", it, 10000, 1.01089, 0.882198, 4, 1, 153);
    test_one<mp>("it", it, 20000, 1.00893, 0.828774, 4, 0, 86); // San Marino disappears

    test_one<mp>("nl", nl, 100, 0.999896, 0.999804, 8, 0, 789);
    test_one<mp>("nl", nl, 200, 0.999733, 0.999095, 8, 0, 633);
    test_one<mp>("nl", nl, 500, 0.999423, 0.996313, 8, 0, 436);
    test_one<mp>("nl", nl, 1000, 0.997893, 0.991951, 8, 0, 331);
    test_one<mp>("nl", nl, 2000, 0.996129, 0.981998, 8, 0, 234);
    test_one<mp>("nl", nl, 5000, 0.986128, 0.896, 5, 0, 132);
    test_one<mp>("nl", nl, 10000, 0.973917, 0.832522, 4, 0, 75);
    test_one<mp>("nl", nl, 20000, 0.970675, 0.739275, 3, 0, 40);

    test_one<mp>("no", no, 100, 0.999966, 0.999975, 95, 0, 7650);
    test_one<mp>("no", no, 200, 0.999812, 0.999731, 95, 0, 6518);
    test_one<mp>("no", no, 500, 0.99929, 0.998092, 95, 0, 4728);
    test_one<mp>("no", no, 1000, 0.998473, 0.994075, 95, 0, 3524);
    test_one<mp>("no", no, 2000, 0.996674, 0.985863, 92, 0, 2576);
    test_one<mp>("no", no, 5000, 0.99098, 0.965689, 87, 0, 1667);
    test_one<mp>("no", no, 10000, 0.978207, 0.906525, 69, 0, 1059);
    test_one<mp>("no", no, 20000, 0.955223, 0.786546, 38, 0, 593);

    test_one<mp>("uk", uk, 100, 0.999942, 0.999878, 48, 0, 3208);
    test_one<mp>("uk", uk, 200, 0.999843, 0.999291, 48, 0, 2615);
    test_one<mp>("uk", uk, 500, 0.999522, 0.996888, 48, 0, 1885);
    test_one<mp>("uk", uk, 1000, 0.999027, 0.992306, 48, 0, 1396);
    test_one<mp>("uk", uk, 2000, 0.998074, 0.983839, 47, 0, 1032);
    test_one<mp>("uk", uk, 5000, 0.991901, 0.943496, 35, 0, 611);
    test_one<mp>("uk", uk, 10000, 0.990039, 0.871969, 23, 0, 359);
    test_one<mp>("uk", uk, 20000, 0.979171, 0.737577, 11, 0, 193);

}

int test_main(int, char* [])
{
    test_all<true, bg::model::point<double, 2, bg::cs::cartesian> >();
    return 0;
}

