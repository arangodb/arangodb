// Boost.Geometry (aka GGL, Generic Geometry Library) // Robustness Test

// Copyright (c) 2013-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Adapted from: the attachment of ticket 9081

#define CHECK_SELF_INTERSECTIONS
#define LIST_WKT

 #include <iomanip>
 #include <iostream>
 #include <vector>
 #include <boost/geometry.hpp>
 #include <boost/geometry/geometries/point_xy.hpp>
 #include <boost/geometry/geometries/polygon.hpp>
 #include <boost/geometry/geometries/register/point.hpp>
 #include <boost/geometry/geometries/register/ring.hpp>
 #include <boost/geometry/io/wkt/wkt.hpp>
 #include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/foreach.hpp>
#include <boost/timer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/geometry/io/svg/svg_mapper.hpp>
#include <fstream>


typedef boost::geometry::model::d2::point_xy<double> pt;
typedef boost::geometry::model::polygon<pt> polygon;
typedef boost::geometry::model::segment<pt> segment;
typedef boost::geometry::model::multi_polygon<polygon> multi_polygon;

template <typename Geometry>
inline void debug_with_svg(int index, char method, Geometry const& a, Geometry const& b, std::string const& headera, std::string const& headerb)
{
    multi_polygon output;
    try
    {
        switch(method)
        {
            case 'i': boost::geometry::intersection(a, b, output); break;
            case 'u': boost::geometry::union_(a, b, output); break;
            case 'd': boost::geometry::difference(a, b, output); break;
            case 'v': boost::geometry::difference(b, a, output); break;
            default : return;
        }
    }
    catch(...)
    {}

    std::ostringstream filename;
    filename << "ticket_9081_" << method << "_" << (1000000 + index) << ".svg";
    std::ofstream svg(filename.str().c_str());

    boost::geometry::svg_mapper<pt> mapper(svg, 400, 400);
    mapper.add(a);
    mapper.add(b);

    mapper.map(a, "fill-opacity:0.5;fill:rgb(153,204,0);stroke:rgb(153,204,0);stroke-width:2");
    mapper.map(b, "fill-opacity:0.3;fill:rgb(51,51,153);stroke:rgb(51,51,153);stroke-width:2");
    BOOST_FOREACH(polygon const& g, output)
    {
        mapper.map(g, "opacity:0.8;fill:none;stroke:rgb(255,128,0);stroke-width:4;stroke-dasharray:1,7;stroke-linecap:round");
    }

    std::ostringstream out;
    out << headera << std::endl << headerb;
    mapper.map(boost::geometry::return_centroid<pt>(a), "fill:rgb(152,204,0);stroke:rgb(153,204,0);stroke-width:0.1", 3);
    mapper.map(boost::geometry::return_centroid<pt>(b), "fill:rgb(51,51,153);stroke:rgb(153,204,0);stroke-width:0.1", 3);
    mapper.text(boost::geometry::return_centroid<pt>(a), headera, "fill:rgb(0,0,0);font-family:Arial;font-size:10px");
    mapper.text(boost::geometry::return_centroid<pt>(b), headerb, "fill:rgb(0,0,0);font-family:Arial;font-size:10px");
}

int main()
{
    int num_orig = 50;
    int num_rounds = 30000;
    srand(1234);
    std::cout << std::setprecision(16);
    std::map<int, std::string> genesis;
    int pj;


    std::string wkt1, wkt2, operation;

    try
    {


    boost::timer t;
    std::vector<multi_polygon> poly_list;

    for(int i=0;i<num_orig;i++)
    {
        multi_polygon mp;
        polygon p;
        for(int j=0;j<3;j++)
        {
            double x=(double)rand()/RAND_MAX;
            double y=(double)rand()/RAND_MAX;
            p.outer().push_back(pt(x,y));
        }
        boost::geometry::correct(p);
        mp.push_back(p);
        boost::geometry::detail::overlay::has_self_intersections(mp);

        std::ostringstream out;
        out << "original " << poly_list.size();
        genesis[poly_list.size()] = out.str();
        poly_list.push_back(mp);

#ifdef LIST_WKT
        std::cout << "Original " << i << " " << boost::geometry::wkt(p) << std::endl;
#endif
    }


    for(int j=0;j<num_rounds;j++)
    {
        if (j % 100 == 0) { std::cout << " " << j; }
        pj = j;
        int a = rand() % poly_list.size();
        int b = rand() % poly_list.size();

        debug_with_svg(j, 'i', poly_list[a], poly_list[b], genesis[a], genesis[b]);

        { std::ostringstream out; out << boost::geometry::wkt(poly_list[a]); wkt1 = out.str(); }
        { std::ostringstream out; out << boost::geometry::wkt(poly_list[b]); wkt2 = out.str(); }

        multi_polygon mp_i, mp_u, mp_d, mp_e;
        operation = "intersection";
        boost::geometry::intersection(poly_list[a],poly_list[b],mp_i);
        operation = "intersection";
        boost::geometry::union_(poly_list[a],poly_list[b],mp_u);
        operation = "difference";
        boost::geometry::difference(poly_list[a],poly_list[b],mp_d);
        boost::geometry::difference(poly_list[b],poly_list[a],mp_e);

#ifdef LIST_WKT
        std::cout << j << std::endl;
        std::cout << "  Genesis a " << genesis[a] << std::endl;
        std::cout << "  Genesis b " << genesis[b] << std::endl;
        std::cout << "  Intersection " << boost::geometry::wkt(mp_i) << std::endl;
        std::cout << "  Difference a " << boost::geometry::wkt(mp_d) << std::endl;
        std::cout << "  Difference b " << boost::geometry::wkt(mp_e) << std::endl;
#endif

#ifdef CHECK_SELF_INTERSECTIONS
        try
        {
            boost::geometry::detail::overlay::has_self_intersections(mp_i);
        }
        catch(...)
        {
            std::cout << "FAILED TO INTERSECT " << j << std::endl;
            std::cout << boost::geometry::wkt(poly_list[a]) << std::endl;
            std::cout << boost::geometry::wkt(poly_list[b]) << std::endl;
            std::cout << boost::geometry::wkt(mp_i) << std::endl;
            try
            {
                boost::geometry::detail::overlay::has_self_intersections(mp_i);
            }
            catch(...)
            {
            }
            break;
        }

        try
        {
            boost::geometry::detail::overlay::has_self_intersections(mp_d);
        }
        catch(...)
        {
            std::cout << "FAILED TO SUBTRACT " << j << std::endl;
            std::cout << boost::geometry::wkt(poly_list[a]) << std::endl;
            std::cout << boost::geometry::wkt(poly_list[b]) << std::endl;
            std::cout << boost::geometry::wkt(mp_d) << std::endl;
            break;
        }
        try
        {
            boost::geometry::detail::overlay::has_self_intersections(mp_e);
        }
        catch(...)
        {
            std::cout << "FAILED TO SUBTRACT " << j << std::endl;
            std::cout << boost::geometry::wkt(poly_list[b]) << std::endl;
            std::cout << boost::geometry::wkt(poly_list[a]) << std::endl;
            std::cout << boost::geometry::wkt(mp_e) << std::endl;
            break;
        }
#endif

        if(boost::geometry::area(mp_i) > 0)
        {
            std::ostringstream out;
            out << j << " intersection(" << genesis[a] << " , " << genesis[b] << ")";
            genesis[poly_list.size()] = out.str();
            poly_list.push_back(mp_i);
        }
        if(boost::geometry::area(mp_d) > 0)
        {
            std::ostringstream out;
            out << j << " difference(" << genesis[a] << " - " << genesis[b] << ")";
            genesis[poly_list.size()] = out.str();
            poly_list.push_back(mp_d);
        }
        if(boost::geometry::area(mp_e) > 0)
        {
            std::ostringstream out;
            out << j << " difference(" << genesis[b] << " - " << genesis[a] << ")";
            genesis[poly_list.size()] = out.str();
            poly_list.push_back(mp_e);
        }
    }

    std::cout << "FINISHED " << t.elapsed() << std::endl;

    }
    catch(std::exception const& e)
    {
        std::cout << e.what()
            << " in " << operation << " at " << pj << std::endl
            << wkt1 << std::endl
            << wkt2 << std::endl
            << std::endl;
    }
    catch(...)
    {
        std::cout << "Other exception" << std::endl;
    }

    return 0;
}
