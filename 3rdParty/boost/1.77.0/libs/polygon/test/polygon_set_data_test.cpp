// Boost.Polygon library polygon_set_data_test.cpp file

//          Copyright Andrii Sydorchuk 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#include <boost/core/lightweight_test.hpp>
#include <boost/polygon/polygon.hpp>
#include <vector>

using namespace boost::polygon;
using namespace boost::polygon::operators;

void polygon_set_data_test1()
{
    typedef point_data<int> point_type;
    typedef polygon_with_holes_data<int> polygon_with_holes_type;
    typedef polygon_set_data<int> polygon_set_type;

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

    BOOST_TEST(!pset.empty());
    BOOST_TEST(!pset.sorted());
    BOOST_TEST(pset.dirty());
    BOOST_TEST_EQ(8, pset.size());

    std::vector<polygon_with_holes_type> vpoly;
    pset.get(vpoly);
    BOOST_TEST_EQ(1, vpoly.size());

    polygon_with_holes_type poly = vpoly[0];
    BOOST_TEST_EQ(5, poly.size());
    BOOST_TEST_EQ(1, poly.size_holes());
}

void polygon_set_data_test2()
{
    typedef point_data<int> point_type;
    typedef polygon_data<int> polygon_type;
    typedef polygon_set_data<int> polygon_set_type;

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

    BOOST_TEST_EQ(4, traps.size());
}

void polygon_set_data_test3()
{
    typedef point_data<int> point_type;
    typedef polygon_data<int> polygon_type;
    typedef polygon_set_data<int> polygon_set_type;

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

    BOOST_TEST_EQ(32.0, area(polygon));
    BOOST_TEST_EQ(32.0, area(polygon));
}

int main()
{
    polygon_set_data_test1();
    polygon_set_data_test2();
    polygon_set_data_test3();
    return boost::report_errors();
}
