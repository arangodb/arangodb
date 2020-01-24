// Boost.Polygon library voronoi_geometry_type_test.cpp file

//          Copyright Andrii Sydorchuk 2010-2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// See http://www.boost.org for updates, documentation, and revision history.

#include <boost/core/lightweight_test.hpp>
#include <boost/polygon/voronoi_geometry_type.hpp>

using namespace boost::polygon;

void source_category_test1()
{
  BOOST_TEST(belongs(SOURCE_CATEGORY_SINGLE_POINT, GEOMETRY_CATEGORY_POINT));
  BOOST_TEST(belongs(SOURCE_CATEGORY_SEGMENT_START_POINT, GEOMETRY_CATEGORY_POINT));
  BOOST_TEST(belongs(SOURCE_CATEGORY_SEGMENT_END_POINT, GEOMETRY_CATEGORY_POINT));
  BOOST_TEST(!belongs(SOURCE_CATEGORY_INITIAL_SEGMENT, GEOMETRY_CATEGORY_POINT));
  BOOST_TEST(!belongs(SOURCE_CATEGORY_REVERSE_SEGMENT, GEOMETRY_CATEGORY_POINT));

  BOOST_TEST(!belongs(SOURCE_CATEGORY_SINGLE_POINT, GEOMETRY_CATEGORY_SEGMENT));
  BOOST_TEST(!belongs(SOURCE_CATEGORY_SEGMENT_START_POINT, GEOMETRY_CATEGORY_SEGMENT));
  BOOST_TEST(!belongs(SOURCE_CATEGORY_SEGMENT_END_POINT, GEOMETRY_CATEGORY_SEGMENT));
  BOOST_TEST(belongs(SOURCE_CATEGORY_INITIAL_SEGMENT, GEOMETRY_CATEGORY_SEGMENT));
  BOOST_TEST(belongs(SOURCE_CATEGORY_REVERSE_SEGMENT, GEOMETRY_CATEGORY_SEGMENT));
}

int main()
{
    source_category_test1();
    return boost::report_errors();
}
