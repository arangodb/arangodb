// Boost.Geometry
// Robustness Test

// Copyright (c) 2019-2021 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_NO_ROBUSTNESS

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

// Basic case. Union should deliver 22.0
static std::string case_a[2] =
    {
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)))",
    "MULTIPOLYGON(((2 7,4 7,4 3,2 3,2 7)))"
    };

// Case with an interior ring. Union should deliver 73.0
static std::string case_b[2] =
    {
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)))",
    "MULTIPOLYGON(((-1 -1,-1 8,8 8,8 -1,-1 -1),(2 7,2 3,4 3,4 7,2 7)))"
    };

// Union should deliver 14.0
static std::string case_c[2] =
    {
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)))",
    "MULTIPOLYGON(((1 1,0 1,0 3,1 3,1 1)))"
    };

struct test_settings
{
    bool verbose{false};
    bool do_output{false};

    // Settings currently not modifiable
    double start_bound{1.0e-2};
    double step_factor{50.0};  // on each side -> 100 steps per factor
    int max_factor{10000};
};

template <bg::overlay_type OverlayType, typename Geometry>
bool test_overlay(std::string const& caseid,
        Geometry const& g1, Geometry const& g2,
        double expected_area,
        test_settings const& settings)
{
    typedef typename boost::range_value<Geometry>::type geometry_out;
    typedef bg::detail::overlay::overlay
        <
            Geometry, Geometry,
            bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value,
            OverlayType == bg::overlay_difference
            ? ! bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value
            : bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value,
            bg::detail::overlay::do_reverse<bg::point_order<Geometry>::value>::value,
            geometry_out,
            OverlayType
        > overlay;

    typedef typename bg::strategies::relate::services::default_strategy
        <
            Geometry, Geometry
        >::type strategy_type;

    strategy_type strategy;

    typedef typename bg::rescale_overlay_policy_type
    <
        Geometry,
        Geometry
    >::type rescale_policy_type;

    rescale_policy_type robust_policy
        = bg::get_rescale_policy<rescale_policy_type>(g1, g2);

    Geometry result;
    bg::detail::overlay::overlay_null_visitor visitor;
    overlay::apply(g1, g2, robust_policy, std::back_inserter(result),
                   strategy, visitor);

    auto const detected_area = bg::area(result);
    if (std::fabs(detected_area - expected_area) > 0.1)
    {
        if (settings.do_output)
        {
            std::cout << "ERROR: " << caseid << std::setprecision(18)
                      << " detected=" << detected_area
                      << " expected=" << expected_area << std::endl
                      << "    " <<  bg::wkt(g1) << std::endl
                      << "    " << bg::wkt(g2) << std::endl;
        }
        return false;
    }
    return true;
}

template <typename Ring>
void update(Ring& ring, double x, double y, std::size_t index)
{
    if (index >= ring.size())
    {
        return;
    }
    bg::set<0>(ring[index], bg::get<0>(ring[index]) + x);
    bg::set<1>(ring[index], bg::get<1>(ring[index]) + y);
    if (index == 0)
    {
        ring.back() = ring.front();
    }
}

template <bg::overlay_type OverlayType, typename MultiPolygon>
std::size_t test_case(std::size_t& error_count,
        std::size_t case_index, std::size_t i, std::size_t j,
        std::size_t min_vertex_index, std::size_t max_vertex_index,
        double offset_x, double offset_y, double expectation,
        MultiPolygon const& poly1, MultiPolygon const& poly2,
        test_settings const settings)
{
    std::size_t n = 0;
    for (std::size_t k = min_vertex_index; k <= max_vertex_index; k++, ++n)
    {
        MultiPolygon poly2_adapted = poly2;

        switch (case_index)
        {
            case 2 :
                update(bg::interior_rings(poly2_adapted.front()).front(), offset_x, offset_y, k);
                break;
            default :
                update(bg::exterior_ring(poly2_adapted.front()), offset_x, offset_y, k);
                break;
        }

        std::ostringstream out;
        out << "case_" << i << "_" << j << "_" << k;
        if (! test_overlay<OverlayType>(out.str(), poly1, poly2_adapted, expectation, settings))
        {
            if (error_count == 0 && ! settings.do_output)
            {
                // First failure is always reported
                test_settings adapted = settings;
                adapted.do_output = true;
                test_overlay<OverlayType>(out.str(), poly1, poly2_adapted, expectation, adapted);
            }
            error_count++;
        }
    }
    return n;
}

template <typename T, bool Clockwise, bg::overlay_type OverlayType>
std::size_t test_all(std::size_t case_index, std::size_t min_vertex_index,
                     std::size_t max_vertex_index,
                     double expectation, test_settings const& settings)
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    typedef bg::model::polygon<point_type, Clockwise> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    const std::string& first = case_a[0];

    const std::string& second
            = case_index == 1 ? case_a[1]
            : case_index == 2 ? case_b[1]
            : case_index == 3 ? case_c[1]
            : "";

    multi_polygon poly1;
    bg::read_wkt(first, poly1);

    multi_polygon poly2;
    bg::read_wkt(second, poly2);

    std::size_t error_count = 0;
    std::size_t n = 0;
    for (int factor = 1; factor < settings.max_factor; factor *= 2)
    {
        std::size_t i = 0;
        double const bound = settings.start_bound / factor;
        double const step = bound / settings.step_factor;
        if (settings.verbose)
        {
            std::cout << "--> use " << bound << " " << step << std::endl;
        }
        for (double offset_x = -bound; offset_x <= bound; offset_x += step, ++i)
        {
            std::size_t j = 0;
            for (double offset_y = -bound; offset_y <= bound; offset_y += step, ++j, ++n)
            {
                n += test_case<OverlayType>(error_count,
                                            case_index, i, j,
                                            min_vertex_index, max_vertex_index,
                                            offset_x, offset_y, expectation,
                                            poly1, poly2, settings);
            }
        }
    }

    std::cout << case_index
            << " #cases: " << n << " #errors: " << error_count << std::endl;
    BOOST_CHECK_EQUAL(error_count, 0u);

    return error_count;
}

int test_main(int argc, char** argv)
{
    BoostGeometryWriteTestConfiguration();
    using coor_t = default_test_type;

    test_settings settings;
    settings.do_output = argc > 2 && atol(argv[2]) == 1;

    // Test three polygons, for the last test two types of intersections
    test_all<coor_t, true, bg::overlay_union>(1, 0, 3, 22.0, settings);
    test_all<coor_t, true, bg::overlay_union>(2, 0, 3, 73.0, settings);
    test_all<coor_t, true, bg::overlay_intersection>(3, 1, 2, 2.0, settings);
    test_all<coor_t, true, bg::overlay_union>(3, 1, 2, 14.0, settings);

    return 0;
}
