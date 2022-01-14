// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <string>

// Instead of having a separate (and nearly similar) unit test to test multipolygons,
// we now include them here and compile them by default. Only undefining the next line
// will avoid testing multi-geometries
#define BOOST_GEOMETRY_UNIT_TEST_MULTI

#include <boost/variant/variant.hpp>

#include <geometry_test_common.hpp>

// The include to test
#include <boost/geometry/algorithms/remove_spikes.hpp>

// Helper includes
#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#if defined(BOOST_GEOMETRY_UNIT_TEST_MULTI)
#  include <boost/geometry/geometries/multi_polygon.hpp>
#endif


#if defined(TEST_WITH_SVG)
#  include <boost/geometry/io/svg/svg_mapper.hpp>
#endif


template <typename Geometry>
inline void test_remove_spikes(std::string const& /*id*/,
            Geometry& geometry,
            double expected_area, double expected_perimeter)
{
    bg::remove_spikes(geometry);

    double detected_area = bg::area(geometry);
    double detected_perimeter = bg::perimeter(geometry);

    BOOST_CHECK_CLOSE(detected_area, expected_area, 0.01);
    BOOST_CHECK_CLOSE(detected_perimeter, expected_perimeter, 0.01);
}

template <typename Geometry>
void test_geometry(std::string const& id, std::string const& wkt,
            double expected_area, double expected_perimeter)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    bg::correct(geometry);
    boost::variant<Geometry> v(geometry);

#if defined(TEST_WITH_SVG)
    std::ostringstream filename;
    filename << "remove_spikes_" << id;
    if (! bg::closure<Geometry>::value)
    {
        filename << "_open";
    }
    filename << ".svg";
    std::ofstream svg(filename.str().c_str());

    bg::svg_mapper<typename bg::point_type<Geometry>::type> mapper(svg, 500, 500);
    mapper.add(geometry);
    mapper.map(geometry, "fill-opacity:0.3;opacity:0.6;fill:rgb(51,51,153);stroke:rgb(0,0,255);stroke-width:2");
#endif

    test_remove_spikes(id, geometry, expected_area, expected_perimeter);
    test_remove_spikes(id, v, expected_area, expected_perimeter);

#if defined(TEST_WITH_SVG)
    mapper.map(geometry, "opacity:0.6;fill:none;stroke:rgb(255,0,0);stroke-width:3");
#endif
}

template <typename P, bool Clockwise, bool Closed>
void test_polygons()
{
    typedef bg::model::ring<P, Clockwise, Closed> ring;
    typedef bg::model::polygon<P, Clockwise, Closed> polygon;

    test_geometry<ring>("box",
            "POLYGON((0 0,0 4,4 4,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("box",
            "POLYGON((0 0,0 4,4 4,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("box2",
            "POLYGON((0 0,0 2,0 4,2 4,4 4,4 2,4 0,2 0,0 0))",
            16, 16);
    test_geometry<polygon>("spike_right",
            "POLYGON((0 0,0 4,4 4,4 2,6 2,4 2,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("spike_at_corner",
            "POLYGON((0 0,0 4,6 4,4 4,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("spike_at_first",
            "POLYGON((0 0,-1 3,0 0,0 4,4 4,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("spike_at_last",
            "POLYGON((0 0,0 4,4 4,4 0,6 0,0 0))",
            16, 16);
    test_geometry<polygon>("spike_at_closing",
            "POLYGON((-1 0,0 0,0 4,4 4,4 0,0 0,-1 0))",
            16, 16);
    test_geometry<polygon>("double_spike",
            "POLYGON((0 0,0 4,4 4,4 2,6 2,5 2,4 2,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("three_double_spike",
            "POLYGON((0 0,0 4,4 4,4 2,6 2,5 2,4.5 2,4 2,4 0,0 0))",
            16, 16);
    test_geometry<polygon>("spike_with_corner",
            "POLYGON((0 0,0 4,4 4,4 2,6 2,6 4,6 2,4 2,4 0,0 0))",
            16, 16);

    test_geometry<polygon>("triangle0",
            "POLYGON((0 0,0 4,2 0,4 0,0 0))",
            4, 6 + sqrt(20.0));
    test_geometry<polygon>("triangle1",
            "POLYGON((0 4,2 0,4 0,0 0,0 4))",
            4, 6 + sqrt(20.0));
    test_geometry<polygon>("triangle2",
            "POLYGON((2 0,4 0,0 0,0 4,2 0))",
            4, 6 + sqrt(20.0));
    test_geometry<polygon>("triangle3",
            "POLYGON((4 0,0 0,0 4,2 0,4 0))",
            4, 6 + sqrt(20.0));

    test_geometry<polygon>("only_spike1",
            "POLYGON((0 0,2 2,0 0))",
            0, 0);
    test_geometry<polygon>("only_spike2",
            "POLYGON((0 0,2 2,4 4,2 2,0 0))",
            0, 0);
    test_geometry<polygon>("only_spike3",
            "POLYGON((0 0,2 2,4 4,0 0))",
            0, 0);
    test_geometry<polygon>("only_spike4",
            "POLYGON((0 0,4 4,2 2,0 0))",
            0, 0);
}


template <typename P, bool Clockwise, bool Closed>
void test_multi_polygons()
{
    typedef bg::model::polygon<P, Clockwise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    test_geometry<multi_polygon>("multi_spike_with_corner",
            "MULTIPOLYGON(((0 0,0 4,4 4,4 2,6 2,6 4,6 2,4 2,4 0,0 0)))",
            16, 16);
}

template <typename P, bool Clockwise, bool Closed>
void test_all()
{
    test_polygons<P, Clockwise, Closed>();
    test_multi_polygons<P, Clockwise, Closed>();
}

int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double>, true, true>();
    test_all<bg::model::d2::point_xy<double>, true, false>();
    return 0;
}

