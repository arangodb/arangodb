// Boost.Geometry
// Robustness Test

// Copyright (c) 2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>

#include <boost/type_traits/is_same.hpp>

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

static std::string case_c[2] =
    {
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)))",
    "MULTIPOLYGON(((1 1,0 1,0 3,1 3,1 1)))"
    };

template <bg::overlay_type OverlayType, typename Geometry>
bool test_overlay(std::string const& caseid,
        Geometry const& g1, Geometry const& g2,
        double expected_area,
        bool do_output)
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

    typedef typename bg::strategy::intersection::services::default_strategy
        <
            typename bg::cs_tag<Geometry>::type
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

    const double detected_area = bg::area(result);
    if (std::fabs(detected_area - expected_area) > 0.01)
    {
        if (do_output)
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
        double x, double y, double expectation,
        MultiPolygon const& poly1, MultiPolygon const& poly2,
        bool do_output)
{
    std::size_t n = 0;
    for (std::size_t k = min_vertex_index; k <= max_vertex_index; k++, ++n)
    {
        MultiPolygon poly2_adapted = poly2;

        switch (case_index)
        {
            case 2 :
                update(bg::interior_rings(poly2_adapted.front()).front(), x, y, k);
                break;
            default :
                update(bg::exterior_ring(poly2_adapted.front()), x, y, k);
                break;
        }

        std::ostringstream out;
        out << "case_" << i << "_" << j << "_" << k;
        if (! test_overlay<OverlayType>(out.str(), poly1, poly2_adapted, expectation, do_output))
        {
            if (error_count == 0)
            {
                // First failure is always reported
                test_overlay<OverlayType>(out.str(), poly1, poly2_adapted, expectation, true);
            }
            error_count++;
        }
    }
    return n;
}

template <typename T, bool Clockwise, bg::overlay_type OverlayType>
std::size_t test_all(std::size_t case_index, std::size_t min_vertex_index,
                     std::size_t max_vertex_index,
                     double expectation, bool do_output)
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
    for (double factor = 1.0; factor > 1.0e-10; factor /= 10.0)
    {
        std::size_t i = 0;
        double const bound = 1.0e-5 * factor;
        double const step = 1.0e-6 * factor;
        for (double x = -bound; x <= bound; x += step, ++i)
        {
            std::size_t j = 0;
            for (double y = -bound; y <= bound; y += step, ++j, ++n)
            {
                n += test_case<OverlayType>(error_count,
                                            case_index, i, j,
                                       min_vertex_index, max_vertex_index,
                                       x, y, expectation,
                                       poly1, poly2, do_output);
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
    typedef double coordinate_type;

    const double factor = argc > 1 ? atof(argv[1]) : 2.0;
    const bool do_output = argc > 2 && atol(argv[2]) == 1;

    std::cout << std::endl << " -> TESTING "
              << string_from_type<coordinate_type>::name()
              << " " << factor
              << std::endl;

    test_all<coordinate_type, true, bg::overlay_union>(1, 0, 3, 22.0, do_output);
    test_all<coordinate_type, true, bg::overlay_union>(2, 0, 3, 73.0, do_output);
    test_all<coordinate_type, true, bg::overlay_intersection>(3, 1, 2, 2.0, do_output);
    test_all<coordinate_type, true, bg::overlay_union>(3, 1, 2, 14.0, do_output);

    return 0;
 }


