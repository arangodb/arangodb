// Boost.Polygon library voronoi_structures_test.cpp file

//          Copyright Andrii Sydorchuk 2010-2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#include <boost/core/lightweight_test.hpp>
#include <boost/polygon/detail/voronoi_structures.hpp>
#include <boost/polygon/voronoi_geometry_type.hpp>
#include <functional>
#include <vector>

using namespace boost::polygon::detail;
using namespace boost::polygon;

typedef point_2d<int> point_type;
typedef site_event<int> site_type;
typedef circle_event<int> circle_type;
typedef ordered_queue<int, std::greater<int> > ordered_queue_type;
typedef beach_line_node_key<int> node_key_type;
typedef beach_line_node_data<int, int> node_data_type;

void point_2d_test1()
{
  point_type p(1, 2);
  BOOST_TEST_EQ(1, p.x());
  BOOST_TEST_EQ(2, p.y());
  p.x(3);
  BOOST_TEST_EQ(3, p.x());
  p.y(4);
  BOOST_TEST_EQ(4, p.y());
}

void site_event_test1()
{
  site_type s(1, 2);
  s.sorted_index(1);
  s.initial_index(2);
  s.source_category(SOURCE_CATEGORY_SEGMENT_START_POINT);
  BOOST_TEST_EQ(1, s.x0());
  BOOST_TEST_EQ(1, s.x1());
  BOOST_TEST_EQ(2, s.y0());
  BOOST_TEST_EQ(2, s.y1());
  BOOST_TEST(s.is_point());
  BOOST_TEST(!s.is_segment());
  BOOST_TEST(!s.is_inverse());
  BOOST_TEST_EQ(1, s.sorted_index());
  BOOST_TEST_EQ(2, s.initial_index());
  BOOST_TEST_EQ(SOURCE_CATEGORY_SEGMENT_START_POINT, s.source_category());
}

void site_event_test2()
{
  site_type s(1, 2, 3, 4);
  s.sorted_index(1);
  s.initial_index(2);
  s.source_category(SOURCE_CATEGORY_INITIAL_SEGMENT);
  BOOST_TEST_EQ(1, s.x0());
  BOOST_TEST_EQ(2, s.y0());
  BOOST_TEST_EQ(3, s.x1());
  BOOST_TEST_EQ(4, s.y1());
  BOOST_TEST(!s.is_point());
  BOOST_TEST(s.is_segment());
  BOOST_TEST(!s.is_inverse());
  BOOST_TEST_EQ(SOURCE_CATEGORY_INITIAL_SEGMENT, s.source_category());

  s.inverse();
  BOOST_TEST_EQ(3, s.x0());
  BOOST_TEST_EQ(4, s.y0());
  BOOST_TEST_EQ(1, s.x1());
  BOOST_TEST_EQ(2, s.y1());
  BOOST_TEST(s.is_inverse());
  BOOST_TEST_EQ(SOURCE_CATEGORY_INITIAL_SEGMENT, s.source_category());
}

void circle_event_test()
{
  circle_type c(0, 1, 2);
  BOOST_TEST_EQ(0, c.x());
  BOOST_TEST_EQ(1, c.y());
  BOOST_TEST_EQ(2, c.lower_x());
  BOOST_TEST_EQ(1, c.lower_y());
  BOOST_TEST(c.is_active());
  c.x(3);
  c.y(4);
  c.lower_x(5);
  BOOST_TEST_EQ(3, c.x());
  BOOST_TEST_EQ(4, c.y());
  BOOST_TEST_EQ(5, c.lower_x());
  BOOST_TEST_EQ(4, c.lower_y());
  c.deactivate();
  BOOST_TEST(!c.is_active());
}

void ordered_queue_test()
{
  ordered_queue_type q;
  BOOST_TEST(q.empty());
  std::vector<int*> vi;
  for (int i = 0; i < 20; ++i)
    vi.push_back(&q.push(i));
  for (int i = 0; i < 20; ++i)
    *vi[i] <<= 1;
  BOOST_TEST(!q.empty());
  for (int i = 0; i < 20; ++i, q.pop())
    BOOST_TEST_EQ(i << 1, q.top());
  BOOST_TEST(q.empty());
}

void beach_line_node_key_test()
{
  node_key_type key(1);
  BOOST_TEST_EQ(1, key.left_site());
  BOOST_TEST_EQ(1, key.right_site());
  key.left_site(2);
  BOOST_TEST_EQ(2, key.left_site());
  BOOST_TEST_EQ(1, key.right_site());
  key.right_site(3);
  BOOST_TEST_EQ(2, key.left_site());
  BOOST_TEST_EQ(3, key.right_site());
}

void beach_line_node_data_test()
{
  node_data_type node_data(NULL);
  BOOST_TEST(node_data.edge() == NULL);
  BOOST_TEST(node_data.circle_event() == NULL);
  int data = 4;
  node_data.circle_event(&data);
  BOOST_TEST(node_data.edge() == NULL);
  BOOST_TEST(node_data.circle_event() == &data);
  node_data.edge(&data);
  BOOST_TEST(node_data.edge() == &data);
  BOOST_TEST(node_data.circle_event() == &data);
}

int main()
{
    point_2d_test1();
    site_event_test1();
    site_event_test2();
    circle_event_test();
    ordered_queue_test();
    beach_line_node_key_test();
    beach_line_node_data_test();
    return boost::report_errors();
}
