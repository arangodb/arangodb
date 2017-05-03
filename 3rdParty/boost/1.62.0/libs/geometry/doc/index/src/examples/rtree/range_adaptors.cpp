// Boost.Geometry Index
//
// Quickbook Examples
//
// Copyright (c) 2011-2014 Adam Wulkiewicz, Lodz, Poland.
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[rtree_range_adaptors

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>

#include <boost/geometry/index/rtree.hpp>

// Boost.Range
#include <boost/range.hpp>
// adaptors
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/transformed.hpp>

// a container
#include <vector>

// just for output
#include <iostream>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

// Define a function object converting a value_type of indexed Range into std::pair<>.
// This is a generic implementation but of course it'd be possible to use some
// specific types. One could also take Value as template parameter and access
// first_type and second_type members, etc.
template <typename First, typename Second>
struct pair_maker
{
    typedef std::pair<First, Second> result_type;
    template<typename T>
    inline result_type operator()(T const& v) const
    {
        return result_type(v.value(), v.index());
    }
};

int main()
{
    typedef bg::model::point<float, 2, bg::cs::cartesian> point;
    typedef bg::model::box<point> box;

    typedef std::vector<box> container;
    typedef container::size_type size_type;

    typedef std::pair<box, size_type> value;

    // create a container of boxes
    container boxes;
    for ( size_type i = 0 ; i < 10 ; ++i )
    {
        // add a box into the container
        box b(point(i + 0.0f, i + 0.0f), point(i + 0.5f, i + 0.5f));
        boxes.push_back(b);
    }

    // create the rtree passing a Range
    bgi::rtree< value, bgi::quadratic<16> >
        rtree(boxes | boost::adaptors::indexed()
                    | boost::adaptors::transformed(pair_maker<box, size_type>()));

    // print the number of values using boxes[0] as indexable
    std::cout << rtree.count(boxes[0]) << std::endl;

    return 0;
}

//]
