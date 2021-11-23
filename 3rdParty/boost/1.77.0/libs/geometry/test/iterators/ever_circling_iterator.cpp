// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <sstream>
#include <string>

#include <geometry_test_common.hpp>

#include <boost/geometry/iterators/ever_circling_iterator.hpp>

#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/register/linestring.hpp>

#include <boost/range/adaptor/transformed.hpp>


template <typename G>
void test_geometry(G const& geo)
{
    typedef typename boost::range_iterator<G const>::type iterator_type;


    // Run 3 times through the geometry
    std::size_t n = boost::size(geo) * 3;

    {
        std::ostringstream out;
        bg::ever_circling_iterator<iterator_type> it(boost::begin(geo), boost::end(geo));
        for (std::size_t i = 0; i < n; ++i, ++it)
        {
            out << bg::get<0>(*it);
        }
        BOOST_CHECK_EQUAL(out.str(), "123451234512345");
    }

    {
        std::ostringstream out;
        // Start somewhere
        bg::ever_circling_iterator<iterator_type> it(
            boost::begin(geo), boost::end(geo), boost::begin(geo) + 1);
        for (std::size_t i = 0; i < n; ++i, ++it)
        {
            out << bg::get<0>(*it);
        }
        BOOST_CHECK_EQUAL(out.str(), "234512345123451");
    }

    {
        std::ostringstream out;

        // Navigate to somewhere
        bg::ever_circling_iterator<iterator_type> it(boost::begin(geo), boost::end(geo));
        for (std::size_t i = 0; i < n; ++i, ++it)
        {
            std::size_t const m = boost::size(geo);
            it.moveto(boost::begin(geo) + m - (i % m) - 1);
            out << bg::get<0>(*it);
        }
        BOOST_CHECK_EQUAL(out.str(), "543215432154321");
    }

    // Check the range_iterator-one
    {
        std::ostringstream out;
        bg::ever_circling_range_iterator<G const> it(geo);
        for (std::size_t i = 0; i < n; ++i, ++it)
        {
            out << bg::get<0>(*it);
        }
        BOOST_CHECK_EQUAL(out.str(), "123451234512345");
    }
}

template <typename G>
void test_geometry(std::string const& wkt)
{
    G geo;
    bg::read_wkt(wkt, geo);
    test_geometry(geo);
}


template <typename P>
P transform_point(P const& p)
{
    P result;
    bg::set<0>(result, bg::get<0>(p) + 1);
    bg::set<1>(result, bg::get<1>(p) + 1);
    return result;
}

template <typename G>
struct transformed_geometry_type
{
    typedef typename bg::point_type<G>::type point_type;
    typedef boost::transformed_range<point_type(*)(point_type const&), G> type;
};

template <typename G>
void test_transformed_geometry(G const& geo)
{
    typedef typename bg::point_type<G>::type point_type;
    test_geometry(geo | boost::adaptors::transformed(&transform_point<point_type>));
}

template <typename G>
void test_transformed_geometry(std::string const& wkt)
{
    G geo;
    bg::read_wkt(wkt, geo);
    test_transformed_geometry(geo);
}


BOOST_GEOMETRY_REGISTER_LINESTRING(transformed_geometry_type< bg::model::linestring< bg::model::d2::point_xy<double> > >::type)

template <typename P>
void test_all()
{
    test_geometry<bg::model::linestring<P> >("linestring(1 1,2 2,3 3,4 4,5 5)");
    test_transformed_geometry<bg::model::linestring<P> >("linestring(0 0,1 1,2 2,3 3,4 4)");
}

int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
