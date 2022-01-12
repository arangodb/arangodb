// Boost.Geometry (aka GGL, Generic Geometry Library)
// Robustness Test

// Copyright (c) 2009-2021 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_NO_BOOST_TEST
#define BOOST_GEOMETRY_NO_ROBUSTNESS
#define BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE

#include <test_overlay_p_q.hpp>

#include <chrono>
#include <fstream>
#include <sstream>

#include <boost/program_options.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>

#include <star_comb.hpp>


template <typename Polygon>
void add(Polygon& polygon, double x, double y, int)
{
    typedef typename boost::geometry::point_type<Polygon>::type p;
    boost::geometry::exterior_ring(polygon).push_back(boost::geometry::make<p>(x, y));
}


template <typename Polygon>
bool test_star_comb(int index, int star_point_count, int comb_comb_count, double factor1, double factor2, bool do_union, p_q_settings const& settings)
{
    Polygon star, comb;
    make_star(star, add<Polygon>, star_point_count, factor1, factor2);
    make_comb(comb, add<Polygon>, comb_comb_count);

    std::ostringstream out;
    out << "star_comb_" << index;
    return test_overlay_p_q
        <
            Polygon,
            typename bg::coordinate_type<Polygon>::type
        >(out.str(), star, comb, settings);
}


template <typename T, bool Clockwise, bool Closed>
void test_all(int count, int star_point_count, int comb_comb_count, double factor1, double factor2, bool do_union, p_q_settings const& settings)
{
    auto const t0 = std::chrono::high_resolution_clock::now();

    typedef bg::model::polygon
        <
            bg::model::d2::point_xy<T>, Clockwise, Closed
        > polygon;

    int errors = 0;
    for(int i = 0; i < count; i++)
    {
        if (! test_star_comb<polygon>(i, star_point_count, comb_comb_count, factor1, factor2, do_union, settings))
        {
            errors++;
        }
    }
    auto const t = std::chrono::high_resolution_clock::now();
    auto const elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t - t0).count();
    std::cout
        << "geometries: " << count
        << " errors: " << errors
        << " type: " << string_from_type<T>::name()
        << " time: " << elapsed_ms / 1000.0 << std::endl;
}

int main(int argc, char** argv)
{
    BoostGeometryWriteTestConfiguration();
    try
    {
        namespace po = boost::program_options;
        po::options_description description("=== star_comb ===\nAllowed options");

        int count = 1;
        bool do_union = false;
        bool ccw = false;
        bool open = false;
        double factor1 = 1.1;
        double factor2 = 0.2;
        int point_count = 50;
        p_q_settings settings;

        description.add_options()
            ("help", "Help message")
            ("count", po::value<int>(&count)->default_value(1), "Number of tests")
            ("point_count", po::value<int>(&point_count)->default_value(1), "Number of points in the star")
            ("diff", po::value<bool>(&settings.also_difference)->default_value(false), "Include testing on difference")
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
            ("ccw", po::value<bool>(&ccw)->default_value(false), "Counter clockwise polygons")
            ("open", po::value<bool>(&open)->default_value(false), "Open polygons")
#endif
            ("wkt", po::value<bool>(&settings.wkt)->default_value(false), "Create a WKT of the inputs, for all tests")
            ("svg", po::value<bool>(&settings.svg)->default_value(false), "Create a SVG for all tests")
        ;

        po::variables_map varmap;
        po::store(po::parse_command_line(argc, argv, description), varmap);
        po::notify(varmap);

        if (varmap.count("help"))
        {
            std::cout << description << std::endl;
            return 1;
        }

        int star_point_count = point_count * 2 + 1;
        int comb_comb_count = point_count;

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
        if (ccw && open)
        {
            test_all<default_test_type, false, false>(count, star_point_count, comb_comb_count, factor1, factor2, do_union, settings);
        }
        else if (ccw)
        {
            test_all<default_test_type, false, true>(count, star_point_count, comb_comb_count, factor1, factor2, do_union, settings);
        }
        else if (open)
        {
            test_all<default_test_type, true, false>(count, star_point_count, comb_comb_count, factor1, factor2, do_union, settings);
        }
        else
#endif
        {
            test_all<default_test_type, true, true>(count, star_point_count, comb_comb_count, factor1, factor2, do_union, settings);
        }
    }
    catch(std::exception const& e)
    {
        std::cout << "Exception " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cout << "Other exception" << std::endl;
    }

    return 0;
}
