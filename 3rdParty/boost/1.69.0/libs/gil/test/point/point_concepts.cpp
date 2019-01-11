//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/concepts.hpp>
#include <boost/gil/point.hpp>

#include <type_traits>

namespace gil = boost::gil;


template <typename Point>
void test_members()
{
    static_assert(Point::num_dimensions == 2U, "point is not 2D");

    using value_t = typename Point::value_type;
    using coord_t = typename Point::template axis<0>::coord_t;
    static_assert(std::is_same<value_t, coord_t>::value,
                  "point and axis type mismatch");
}

int main()
{
    boost::function_requires<gil::PointNDConcept<gil::point<int>>>();
    boost::function_requires<gil::PointNDConcept<gil::point_t>>();

    boost::function_requires<gil::Point2DConcept<gil::point<int>>>();
    boost::function_requires<gil::Point2DConcept<gil::point_t>>();

    test_members<gil::point<int>>();
    test_members<gil::point_t>();

    // NOTE: point2 is deprecated, available for backward compatibility
    boost::function_requires<gil::PointNDConcept<gil::point2<int>>>();
    boost::function_requires<gil::Point2DConcept<gil::point2<int>>>();
    test_members<gil::point2<int>>();

    return 0;
}
