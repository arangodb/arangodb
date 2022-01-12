// Boost.Geometry (aka GGL, Generic Geometry Library)
// Performance Test

// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/timer.hpp>

#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/write.hpp>
#include <boost/geometry/strategies/strategies.hpp>


namespace bg = boost::geometry;

template
<
    typename GeometryOut,
    typename Geometry
>
double test_growth(Geometry const& geometry, int n, int d, double distance)
{

    typedef typename bg::coordinate_type<Geometry>::type coordinate_type;
    typedef typename bg::point_type<Geometry>::type point_type;

    // extern int point_buffer_count;
    std::ostringstream complete;
    complete
        << "point_growth"
        << "_" << "r"
        << "_" << n
        << "_" << d
         // << "_" << point_buffer_count
        ;

    //std::cout << complete.str() << std::endl;

    std::ostringstream filename;
    filename << "buffer_" << complete.str() << ".svg";

    std::ofstream svg(filename.str().c_str());

#ifdef BOOST_GEOMETRY_DEBUG_WITH_MAPPER
    bg::svg_mapper<point_type> mapper(svg, 500, 500);

    {
        bg::model::box<point_type> box;
        bg::envelope(geometry, box);

        bg::buffer(box, box, distance * 1.01);
        mapper.add(box);
    }
#endif

    bg::strategy::buffer::join_round join_strategy(100);
    bg::strategy::buffer::end_flat end_strategy;
    bg::strategy::buffer::point_circle point_strategy;
    bg::strategy::buffer::side_straight side_strategy;

    typedef bg::strategy::buffer::distance_symmetric<coordinate_type> distance_strategy_type;
    distance_strategy_type distance_strategy(distance);

    std::vector<GeometryOut> buffered;

    bg::buffer(geometry, buffered,
                distance_strategy, side_strategy,
                join_strategy, end_strategy, point_strategy);


    typename bg::default_area_result<GeometryOut>::type area = 0;
    for (GeometryOut const& polygon : buffered)
    {
        area += bg::area(polygon);
    }

#ifdef BOOST_GEOMETRY_DEBUG_WITH_MAPPER
    // Map input geometry in green
    mapper.map(geometry, "opacity:0.5;fill:rgb(0,128,0);stroke:rgb(0,128,0);stroke-width:10");

    for (GeometryOut const& polygon : buffered)
    {
        mapper.map(polygon, "opacity:0.4;fill:rgb(255,255,128);stroke:rgb(0,0,0);stroke-width:3");
    }
#endif

    return area;
}

template <typename P>
void test_growth(int n, int distance_count)
{
    srand(int(time(NULL)));
    //std::cout << typeid(bg::coordinate_type<P>::type).name() << std::endl;
    boost::timer t;

    namespace buf = bg::strategy::buffer;
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::multi_point<P> multi_point_type;

    multi_point_type multi_point;
    for (int i = 0; i < n; i++)
    {
        P point(rand() % 100, rand() % 100);
        multi_point.push_back(point);
    }

    //std::cout << bg::wkt(multi_point) << std::endl;

    double previous_area = 0;
    double epsilon = 0.1;
    double distance = 15.0;
    for (int d = 0; d < distance_count; d++, distance += epsilon)
    {
        double area = test_growth<polygon>(multi_point, n, d, distance);
        if (area < previous_area)
        {
            std::cout << "Error: " << area << " < " << previous_area << std::endl
                << " n=" << n << " distance=" << distance
                << bg::wkt(multi_point) << std::endl;
        }
        previous_area = area;
    }
    std::cout << "n=" << n << " time=" << t.elapsed() << std::endl;
}

int main(int, char* [])
{
    for (int i = 5; i <= 50; i++)
    {
        test_growth<bg::model::point<double, 2, bg::cs::cartesian> >(i, 20);
    }

    return 0;
}
