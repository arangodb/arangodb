// Boost.Geometry
// Unit Test

// Copyright (c) 2016 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_WITH_SVG
#include <fstream>
#endif

#include <sstream>
#include <string>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/correct.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/geometry/io/svg/svg_mapper.hpp>
#include <boost/geometry/io/svg/write_svg.hpp>
#include <boost/geometry/io/svg/write_svg_multi.hpp>

#include <boost/geometry/strategies/cartesian/area_surveyor.hpp>

template <typename R, typename T>
inline void push_back_square(R & rng, T const& mi, T const& ma)
{
    typedef typename bg::point_type<R>::type P;
    rng.push_back(P(mi, mi));
    rng.push_back(P(mi, ma));
    rng.push_back(P(ma, ma));
    rng.push_back(P(ma, mi));
    rng.push_back(P(mi, mi));
}

template <typename P>
void test_all()
{
    typedef bg::model::box<P> box;
    typedef bg::model::segment<P> segment;
    typedef bg::model::linestring<P> linestring;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> polygon;

    typedef bg::model::multi_point<P> multi_point;
    typedef bg::model::multi_linestring<linestring> multi_linestring;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    
    P pt(0, 0);
    box b(P(10, 10), P(20, 20));
    segment s(P(30, 30), P(40, 40));
    
    linestring ls;
    push_back_square(ls, 50, 60);
    
    ring r;
    push_back_square(r, 70, 80);

    polygon po;
    push_back_square(po.outer(), 90, 120);
    po.inners().resize(1);
    push_back_square(po.inners()[0], 100, 110);
    bg::correct(po);

    multi_point m_pt;
    m_pt.push_back(pt);

    multi_linestring m_ls;
    m_ls.push_back(ls);

    multi_polygon m_po;
    m_po.push_back(po);

    std::string style = "fill-opacity:0.5;fill:rgb(200,0,0);stroke:rgb(200,0,0);stroke-width:3";
    std::string m_style = "fill-opacity:0.5;fill:rgb(0,200,0);stroke:rgb(0,200,0);stroke-width:1";

    {
#ifdef TEST_WITH_SVG
        std::ofstream os("test1.svg", std::ios::trunc);
#else
        std::stringstream os;
#endif
        os << "<?xml version=\"1.0\" standalone=\"no\"?>"
           << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">"
           << "<svg height=\"200\" width=\"200\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">";

        os << bg::svg(pt, style);
        os << bg::svg(b, style);
        os << bg::svg(s, style);
        os << bg::svg(ls, style);
        os << bg::svg(r, style);
        os << bg::svg(po, style);
        os << bg::svg(m_pt, m_style);
        os << bg::svg(m_ls, m_style);
        os << bg::svg(m_po, m_style);

        os << "</svg>";
    }

    {
#ifdef TEST_WITH_SVG
        std::ofstream os("test2.svg", std::ios::trunc);
#else
        std::stringstream os;
#endif
        bg::svg_mapper<P> mapper(os, 500, 500);
        mapper.add(pt);
        mapper.add(b);
        mapper.add(s);
        mapper.add(ls);
        mapper.add(r);
        mapper.add(po);
        mapper.add(m_pt);
        mapper.add(m_ls);
        mapper.add(m_po);
        mapper.map(pt, style);
        mapper.map(b, style);
        mapper.map(s, style);
        mapper.map(ls, style);
        mapper.map(r, style);
        mapper.map(po, style);
        mapper.map(m_pt, m_style);
        mapper.map(m_ls, m_style);
        mapper.map(m_po, m_style);
    }
}

int test_main(int, char* [])
{
    test_all< boost::geometry::model::d2::point_xy<double> >();
    test_all< boost::geometry::model::d2::point_xy<int> >();

#if defined(HAVE_TTMATH)
    test_all< boost::geometry::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
