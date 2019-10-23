/*
Copyright 2008 Intel Corporation

Use, modification and distribution are subject to the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt).
*/
#include <boost/polygon/polygon.hpp>
#include <list>
#include <time.h>
#include <cassert>
#include <deque>
#include <iostream>
namespace gtl = boost::polygon;
using namespace boost::polygon::operators;

//once again we make our usage of the library generic
//and parameterize it on the polygon set type
template <typename PolygonSet>
void test_polygon_set() {
  using namespace gtl; 
  PolygonSet ps;
  ps += rectangle_data<int>(0, 0, 10, 10);
  PolygonSet ps2;
  ps2 += rectangle_data<int>(5, 5, 15, 15);
  PolygonSet ps3;
  assign(ps3, ps * ps2); 
  PolygonSet ps4;
  ps4 += ps + ps2;
  assert(area(ps4) == area(ps) + area(ps2) - area(ps3));
  assert(equivalence((ps + ps2) - (ps * ps2), ps ^ ps2));
  rectangle_data<int> rect;
  assert(extents(rect, ps ^ ps2));
  assert(area(rect) == 225);
  assert(area(rect ^ (ps ^ ps2)) == area(rect) - area(ps ^ ps2)); 
}

//first thing is first, lets include all the code from previous examples

//the CPoint example
struct CPoint {
  int x;
  int y;
};

namespace boost { namespace polygon {
    template <>
    struct geometry_concept<CPoint> { typedef point_concept type; };
    template <>
    struct point_traits<CPoint> {
      typedef int coordinate_type;

      static inline coordinate_type get(const CPoint& point, 
					orientation_2d orient) {
	if(orient == HORIZONTAL)
	  return point.x;
	return point.y;
      }
    };

    template <>
    struct point_mutable_traits<CPoint> {
      typedef int coordinate_type;

      static inline void set(CPoint& point, orientation_2d orient, int value) {
	if(orient == HORIZONTAL)
	  point.x = value;
	else
	  point.y = value;
      }
      static inline CPoint construct(int x_value, int y_value) {
	CPoint retval;
	retval.x = x_value;
	retval.y = y_value; 
	return retval;
      }
    };
  } }

//the CPolygon example
typedef std::list<CPoint> CPolygon;

//we need to specialize our polygon concept mapping in boost polygon
namespace boost { namespace polygon {
    //first register CPolygon as a polygon_concept type
    template <>
    struct geometry_concept<CPolygon>{ typedef polygon_concept type; };

    template <>
    struct polygon_traits<CPolygon> {
      typedef int coordinate_type;
      typedef CPolygon::const_iterator iterator_type;
      typedef CPoint point_type;

      // Get the begin iterator
      static inline iterator_type begin_points(const CPolygon& t) {
	return t.begin();
      }

      // Get the end iterator
      static inline iterator_type end_points(const CPolygon& t) {
	return t.end();
      }

      // Get the number of sides of the polygon
      static inline std::size_t size(const CPolygon& t) {
	return t.size();
      }

      // Get the winding direction of the polygon
      static inline winding_direction winding(const CPolygon& t) {
	return unknown_winding;
      }
    };

    template <>
    struct polygon_mutable_traits<CPolygon> {
      //expects stl style iterators
      template <typename iT>
      static inline CPolygon& set_points(CPolygon& t, 
					 iT input_begin, iT input_end) {
	t.clear();
	while(input_begin != input_end) {
	  t.push_back(CPoint());
	  gtl::assign(t.back(), *input_begin);
	  ++input_begin;
	}
	return t;
      }

    };
  } }

//OK, finally we get to declare our own polygon set type
typedef std::deque<CPolygon> CPolygonSet;

//deque isn't automatically a polygon set in the library
//because it is a standard container there is a shortcut
//for mapping it to polygon set concept, but I'll do it
//the long way that you would use in the general case.
namespace boost { namespace polygon {
    //first we register CPolygonSet as a polygon set
    template <>
    struct geometry_concept<CPolygonSet> { typedef polygon_set_concept type; };

    //next we map to the concept through traits
    template <>
    struct polygon_set_traits<CPolygonSet> {
      typedef int coordinate_type;
      typedef CPolygonSet::const_iterator iterator_type;
      typedef CPolygonSet operator_arg_type;

      static inline iterator_type begin(const CPolygonSet& polygon_set) {
	return polygon_set.begin();
      }

      static inline iterator_type end(const CPolygonSet& polygon_set) {
	return polygon_set.end();
      }

      //don't worry about these, just return false from them
      static inline bool clean(const CPolygonSet& polygon_set) { return false; }
      static inline bool sorted(const CPolygonSet& polygon_set) { return false; }
    };

    template <>
    struct polygon_set_mutable_traits<CPolygonSet> {
      template <typename input_iterator_type>
      static inline void set(CPolygonSet& polygon_set, input_iterator_type input_begin, input_iterator_type input_end) {
	polygon_set.clear();
	//this is kind of cheesy. I am copying the unknown input geometry
	//into my own polygon set and then calling get to populate the
	//deque
	polygon_set_data<int> ps;
	ps.insert(input_begin, input_end);
	ps.get(polygon_set);
	//if you had your own odd-ball polygon set you would probably have
	//to iterate through each polygon at this point and do something
	//extra
      }
    };
} }

int main() {
  long long c1 = clock();
  for(int i = 0; i < 1000; ++i) 
    test_polygon_set<CPolygonSet>();
  long long c2 = clock();
  for(int i = 0; i < 1000; ++i) 
    test_polygon_set<gtl::polygon_set_data<int> >();
  long long c3 = clock();
  long long diff1 = c2 - c1;
  long long diff2 = c3 - c2;
  if(diff1 > 0 && diff2)
    std::cout << "library polygon_set_data is " << float(diff1)/float(diff2) << "X faster than custom polygon set deque of CPolygon" << std::endl;
  else
    std::cout << "operation was too fast" << std::endl;
  return 0;
}

//Now you know how to map your own data type to polygon set concept
//Now you also know how to make your application code that operates on geometry
//data type agnostic from point through polygon set
