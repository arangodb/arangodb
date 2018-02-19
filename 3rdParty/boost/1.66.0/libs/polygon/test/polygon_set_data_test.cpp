// Boost.Polygon library polygon_set_data_test.cpp file

//          Copyright Andrii Sydorchuk 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#define BOOST_TEST_MODULE POLYGON_SET_DATA_TEST
#include <vector>

#include <boost/mpl/list.hpp>
#include <boost/test/test_case_template.hpp>

#include "boost/polygon/polygon.hpp"
using namespace boost::polygon;
using namespace boost::polygon::operators;

typedef boost::mpl::list<int> test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(polygon_set_data_test1, T, test_types) {
    typedef point_data<T> point_type;
    typedef polygon_data<T> polygon_type;
    typedef polygon_with_holes_data<T> polygon_with_holes_type;
    typedef polygon_set_data<T> polygon_set_type;

    polygon_set_type pset;
    std::vector<point_type> outbox;
    outbox.push_back(point_type(0, 0));
    outbox.push_back(point_type(100, 0));
    outbox.push_back(point_type(100, 100));
    outbox.push_back(point_type(0, 100));
    pset.insert_vertex_sequence(outbox.begin(), outbox.end(), COUNTERCLOCKWISE, false);
    std::vector<point_type> inbox;
    inbox.push_back(point_type(20, 20));
    inbox.push_back(point_type(80, 20));
    inbox.push_back(point_type(80, 80));
    inbox.push_back(point_type(20, 80));
    pset.insert_vertex_sequence(inbox.begin(), inbox.end(), COUNTERCLOCKWISE, true);

    BOOST_CHECK(!pset.empty());
    BOOST_CHECK(!pset.sorted());
    BOOST_CHECK(pset.dirty());
    BOOST_CHECK_EQUAL(8, pset.size());

    std::vector<polygon_with_holes_type> vpoly;
    pset.get(vpoly);
    BOOST_CHECK_EQUAL(1, vpoly.size());

    polygon_with_holes_type poly = vpoly[0];
    BOOST_CHECK_EQUAL(5, poly.size());
    BOOST_CHECK_EQUAL(1, poly.size_holes());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(polygon_set_data_test2, T, test_types) {
    typedef point_data<T> point_type;
    typedef polygon_data<T> polygon_type;
    typedef polygon_with_holes_data<T> polygon_with_holes_type;
    typedef polygon_set_data<T> polygon_set_type;

    std::vector<point_type> data;
    data.push_back(point_type(2,0));
    data.push_back(point_type(4,0));
    data.push_back(point_type(4,3));
    data.push_back(point_type(0,3));
    data.push_back(point_type(0,0));
    data.push_back(point_type(2,0));
    data.push_back(point_type(2,1));
    data.push_back(point_type(1,1));
    data.push_back(point_type(1,2));
    data.push_back(point_type(3,2));
    data.push_back(point_type(3,1));
    data.push_back(point_type(2,1));
    data.push_back(point_type(2,0));

    polygon_type polygon;
    set_points(polygon, data.begin(), data.end());

    polygon_set_type pset;
    pset.insert(polygon);

    std::vector<polygon_type> traps;
    get_trapezoids(traps, pset, HORIZONTAL);

    BOOST_CHECK_EQUAL(4, traps.size());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(polygon_set_data_test3, T, test_types) {
    typedef point_data<T> point_type;
    typedef polygon_data<T> polygon_type;
    typedef polygon_with_holes_data<T> polygon_with_holes_type;
    typedef polygon_set_data<T> polygon_set_type;

    std::vector<point_type> data;
    data.push_back(point_type(0,0));
    data.push_back(point_type(6,0));
    data.push_back(point_type(6,4));
    data.push_back(point_type(4,6));
    data.push_back(point_type(0,6));
    data.push_back(point_type(0,0));
    data.push_back(point_type(4,4));
    data.push_back(point_type(5,4));

    polygon_type polygon(data.begin(), data.end());
    polygon_set_type pset;
    pset += polygon;

    BOOST_CHECK_EQUAL(32.0, area(polygon));
    BOOST_CHECK_EQUAL(32.0, area(polygon));
}
