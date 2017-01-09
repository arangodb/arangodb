// Boost.Polygon library polygon_rectangle_test.cpp file

//          Copyright Andrii Sydorchuk 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#define BOOST_TEST_MODULE POLYGON_RECTANGLE_TEST
#include <boost/mpl/list.hpp>
#include <boost/test/test_case_template.hpp>

#include "boost/polygon/rectangle_concept.hpp"
#include "boost/polygon/rectangle_data.hpp"
#include "boost/polygon/rectangle_traits.hpp"
using namespace boost::polygon;

typedef boost::mpl::list<int> test_types;

template <typename interval_type>
void CHECK_INTERVAL_EQUAL(const interval_type& i1, const interval_type& i2) {
  BOOST_CHECK_EQUAL(get(i1, LOW), get(i2, LOW));
  BOOST_CHECK_EQUAL(get(i1, HIGH), get(i2, HIGH));
}

template <typename rectangle_type>
void CHECK_RECT_EQUAL(const rectangle_type& r1, const rectangle_type& r2) {
  CHECK_INTERVAL_EQUAL(horizontal(r1), horizontal(r2));
  CHECK_INTERVAL_EQUAL(vertical(r1), vertical(r2));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(rectangle_concept_test1, T, test_types) {
  typedef rectangle_data<T> rectangle_type;

  rectangle_type rectangle1 = construct<rectangle_type>(-1, -1, 1, 1);
  scale_up(rectangle1, 2);
  CHECK_RECT_EQUAL(construct<rectangle_type>(-2, -2, 2, 2), rectangle1);

  scale_down(rectangle1, 2);
  CHECK_RECT_EQUAL(construct<rectangle_type>(-1, -1, 1, 1), rectangle1);
}
