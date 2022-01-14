// Boost.Geometry (aka GGL, Generic Geometry Library)
// Robustness Test
//
// Copyright (c) 2009-2020 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_OVERLAY_P_Q_HPP
#define BOOST_GEOMETRY_TEST_OVERLAY_P_Q_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <boost/typeof/typeof.hpp>

#include <geometry_test_common.hpp>

// For mixing int/float
#if defined(_MSC_VER)
#pragma warning( disable : 4244 )
#pragma warning( disable : 4267 )
#endif


#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/io/svg/svg_mapper.hpp>

#include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/touches.hpp>

struct p_q_settings
{
    bool svg;
    bool also_difference;
    bool validity;
    bool wkt;
    bool verify_area;
    double tolerance;

    p_q_settings()
        : svg(false)
        , also_difference(false)
        , validity(false)
        , wkt(false)
        , verify_area(false)
        , tolerance(1.0e-3) // since rescaling to integer the tolerance should be less. Was originally 1.0e-6
    {}
};

template <typename Geometry>
inline typename bg::default_area_result<Geometry>::type p_q_area(Geometry const& g)
{
    try
    {
        return bg::area(g);
    }
    catch(bg::empty_input_exception const&)
    {
        return 0;
    }
}

struct verify_area
{
    template <typename Iterator>
    static inline bool check_ring(Iterator begin, Iterator end)
    {
        for (Iterator it = begin; it != end; ++it)
        {
            auto const area = bg::area(*it);
            if (bg::math::abs(area) < 0.01)
            {
                return false;
            }
        }
        return true;
    }

    template <typename Interiors>
    static inline bool check_rings(Interiors const& rings)
    {
        return check_ring(boost::begin(rings), boost::end(rings));
    }

    template <typename Iterator>
    static inline bool check_polys(Iterator begin, Iterator end)
    {
        for (Iterator it = begin; it != end; ++it)
        {
            // If necessary, exterior_ring can be checked too
            if (! check_rings(bg::interior_rings(*it)))
            {
                return false;
            }
        }
        return true;
    }

    template <typename Geometry>
    static inline bool apply(Geometry const& g)
    {
        return check_polys(boost::begin(g), boost::end(g));
    }
};

template <typename OutputType, typename CalculationType, typename G1, typename G2>
static bool test_overlay_p_q(std::string const& caseid,
            G1 const& p, G2 const& q,
            p_q_settings const& settings)
{
    bool result = true;

    typedef typename bg::coordinate_type<G1>::type coordinate_type;
    typedef typename bg::point_type<G1>::type point_type;

    bg::model::multi_polygon<OutputType> out_i, out_u, out_d1, out_d2;

    CalculationType area_p = p_q_area(p);
    CalculationType area_q = p_q_area(q);
    CalculationType area_d1 = 0, area_d2 = 0;

    bg::intersection(p, q, out_i);
    CalculationType area_i = p_q_area(out_i);

    bg::union_(p, q, out_u);
    CalculationType area_u = p_q_area(out_u);

    auto const sum = (area_p + area_q) - area_u - area_i;

    bool wrong = bg::math::abs(sum) > settings.tolerance;

    if (settings.also_difference)
    {
        bg::difference(p, q, out_d1);
        bg::difference(q, p, out_d2);
        area_d1 = p_q_area(out_d1);
        area_d2 = p_q_area(out_d2);
        auto sum_d1 = (area_u - area_q) - area_d1;
        auto sum_d2 = (area_u - area_p) - area_d2;
        bool wrong_d1 = bg::math::abs(sum_d1) > settings.tolerance;
        bool wrong_d2 = bg::math::abs(sum_d2) > settings.tolerance;

        if (wrong_d1 || wrong_d2)
        {
            wrong = true;
        }
    }

    if (settings.validity)
    {
        std::string message;
        if (! bg::is_valid(out_u, message))
        {
            std::cout << "Union is not valid: " << message << std::endl;
            wrong = true;
        }
        if (! bg::is_valid(out_i, message))
        {
            std::cout << "Intersection is not valid: " << message << std::endl;
            wrong = true;
        }
        if (settings.also_difference)
        {
            if (! bg::is_valid(out_d1, message))
            {
                std::cout << "Difference (p-q) is not valid: " << message << std::endl;
                wrong = true;
            }
            if (! bg::is_valid(out_d2, message))
            {
                std::cout << "Difference (q-p) is not valid: " << message << std::endl;
                wrong = true;
            }
        }

        if (settings.verify_area && ! verify_area::apply(out_u))
        {
            std::cout << "Union/interior area incorrect" << std::endl;
            wrong = true;
        }
        if (settings.verify_area && ! verify_area::apply(out_i))
        {
            std::cout << "Intersection/interior area incorrect" << std::endl;
            wrong = true;
        }
    }

    if (true)
    {
        if ((area_i > 0 && bg::touches(p, q))
            || (area_i <= 0 && bg::intersects(p, q) && ! bg::touches(p, q)))
        {
            std::cout << "Wrong 'touch'! "
                << " Intersection area: " << area_i
                << " Touch gives: " << std::boolalpha << bg::touches(p, q)
                << std::endl;
            wrong = true;
        }
    }

    bool svg = settings.svg;

    if (wrong || settings.wkt)
    {
        if (wrong)
        {
            result = false;
            svg = true;
        }
        bg::unique(out_i);
        bg::unique(out_u);

        std::cout
            << "type: " << string_from_type<CalculationType>::name()
            << " id: " << caseid
            << " area i: " << area_i
            << " area u: " << area_u
            << " area p: " << area_p
            << " area q: " << area_q
            << " sum: " << sum;

        if (settings.also_difference)
        {
            std::cout
                << " area d1: " << area_d1
                << " area d2: " << area_d2;
        }
        std::cout
            << std::endl
            << std::setprecision(9)
            << " p: " << bg::wkt(p) << std::endl
            << " q: " << bg::wkt(q) << std::endl
            << " i: " << bg::wkt(out_i) << std::endl
            << " u: " << bg::wkt(out_u) << std::endl
            ;

    }

    if(svg)
    {
        std::ostringstream filename;
        filename << "overlay_" << caseid << "_"
            << string_from_type<coordinate_type>::name();
        if (!std::is_same<coordinate_type, CalculationType>::value)
        {
            filename << string_from_type<CalculationType>::name();
        }

        filename
#if defined(BOOST_GEOMETRY_USE_RESCALING)
            << "_rescaled"
#endif
            << ".svg";

        std::ofstream svg(filename.str().c_str());

        bg::svg_mapper<point_type> mapper(svg, 500, 500);

        mapper.add(p);
        mapper.add(q);

        // Input shapes in green/blue
        mapper.map(p, "fill-opacity:0.5;fill:rgb(153,204,0);"
                "stroke:rgb(153,204,0);stroke-width:3");
        mapper.map(q, "fill-opacity:0.3;fill:rgb(51,51,153);"
                "stroke:rgb(51,51,153);stroke-width:3");

        if (settings.also_difference)
        {
            for (BOOST_AUTO(it, out_d1.begin()); it != out_d1.end(); ++it)
            {
                mapper.map(*it,
                    "opacity:0.8;fill:none;stroke:rgb(255,128,0);stroke-width:4;stroke-dasharray:1,7;stroke-linecap:round");
            }
            for (BOOST_AUTO(it, out_d2.begin()); it != out_d2.end(); ++it)
            {
                mapper.map(*it,
                    "opacity:0.8;fill:none;stroke:rgb(255,0,255);stroke-width:4;stroke-dasharray:1,7;stroke-linecap:round");
            }
        }
        else
        {
            mapper.map(out_i, "fill-opacity:0.1;stroke-opacity:0.4;fill:rgb(255,0,128);"
                    "stroke:rgb(255,0,0);stroke-width:4");
            mapper.map(out_u, "fill-opacity:0.1;stroke-opacity:0.4;fill:rgb(255,0,0);"
                    "stroke:rgb(255,0,255);stroke-width:4");
        }
    }
    return result;
}

#endif // BOOST_GEOMETRY_TEST_OVERLAY_P_Q_HPP
