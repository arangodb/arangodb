// Boost.Polygon library polygon_rectangle_formation_test.cpp file

//          Copyright Andrii Sydorchuk 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#define BOOST_TEST_MODULE POLYGON_RECTANGLE_FORMATION_TEST

#include <boost/mpl/list.hpp>
#include <boost/test/test_case_template.hpp>

#include "boost/polygon/polygon.hpp"
using namespace boost::polygon;

typedef boost::mpl::list<int> test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(rectangle_formation_test1, T, test_types) {
  typedef polygon_90_with_holes_data<int> polygon_type;
  typedef polygon_traits<polygon_type>::point_type point_type;

  polygon_type poly;
  point_type points[] = {
    boost::polygon::construct<point_type>(0, 0),
    boost::polygon::construct<point_type>(0, 10),
    boost::polygon::construct<point_type>(10, 10),
    boost::polygon::construct<point_type>(10, 0),
  };
  boost::polygon::set_points(poly, points, points + 4);

  std::vector< rectangle_data<int> > rects;
  boost::polygon::get_rectangles(rects, poly);

  BOOST_CHECK_EQUAL(1, rects.size());
  const rectangle_data<int>& rect = rects[0];
  BOOST_CHECK_EQUAL(0, rect.get(WEST));
  BOOST_CHECK_EQUAL(10, rect.get(EAST));
  BOOST_CHECK_EQUAL(10, rect.get(NORTH));
  BOOST_CHECK_EQUAL(0, rect.get(SOUTH));
}
