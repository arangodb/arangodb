// Boost.Polygon library polygon_90_data_test.cpp file

//          Copyright Andrii Sydorchuk 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#define BOOST_TEST_MODULE POLYGON_90_DATA_TEST
#include <vector>

#include <boost/mpl/list.hpp>
#include <boost/test/test_case_template.hpp>

#include "boost/polygon/polygon.hpp"
using namespace boost::polygon;

#include <iostream>

typedef boost::mpl::list<int> test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(polygon_90_data_test, T, test_types) {
    typedef polygon_90_data<int> polygon_type;
    typedef polygon_traits_90<polygon_type>::point_type point_type;
    typedef polygon_type::iterator_type iterator_type;

    std::vector<point_type> data;
    data.push_back(point_type(0, 0)); // 1
    data.push_back(point_type(10, 0)); // 2
    data.push_back(point_type(10, 10)); // 3
    data.push_back(point_type(0, 10)); // 4

    polygon_type polygon;
    polygon.set(data.begin(), data.end());

    std::cout << "Interesting: " << std::endl;
    for (polygon_type::compact_iterator_type it = polygon.begin_compact(); it != polygon.end_compact(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    iterator_type it = polygon.begin();
    for (int i = 0; i < 2; i++) {
        it++;
    }

    iterator_type it_3rd = it;
    it++;
    BOOST_CHECK(it != it_3rd);
}
