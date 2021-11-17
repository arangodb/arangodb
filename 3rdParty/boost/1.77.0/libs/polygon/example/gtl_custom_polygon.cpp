/*
Copyright 2008 Intel Corporation

Use, modification and distribution are subject to the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt).
*/
#include <boost/polygon/polygon.hpp>
#include <cassert>
#include <list>
namespace gtl = boost::polygon;
using namespace boost::polygon::operators;

//first lets turn our polygon usage code into a generic
//function parameterized by polygon type
template <typename Polygon>
void test_polygon() {
  //lets construct a 10x10 rectangle shaped polygon
  typedef typename gtl::polygon_traits<Polygon>::point_type Point;
  Point pts[] = {gtl::construct<Point>(0, 0),
		 gtl::construct<Point>(10, 0),
		 gtl::construct<Point>(10, 10),
		 gtl::construct<Point>(0, 10) };
  Polygon poly;
  gtl::set_points(poly, pts, pts+4);

  //now lets see what we can do with this polygon
  assert(gtl::area(poly) == 100.0f);
  assert(gtl::contains(poly, gtl::construct<Point>(5, 5)));
  assert(!gtl::contains(poly, gtl::construct<Point>(15, 5)));
  gtl::rectangle_data<int> rect;
  assert(gtl::extents(rect, poly)); //get bounding box of poly
  assert(gtl::equivalence(rect, poly)); //hey, that's slick
  assert(gtl::winding(poly) == gtl::COUNTERCLOCKWISE);
  assert(gtl::perimeter(poly) == 40.0f);

  //add 5 to all coords of poly
  gtl::convolve(poly, gtl::construct<Point>(5, 5));
  //multiply all coords of poly by 2
  gtl::scale_up(poly, 2);
  gtl::set_points(rect, gtl::point_data<int>(10, 10),
		  gtl::point_data<int>(30, 30));
  assert(gtl::equivalence(poly, rect));
}

//Now lets declare our own polygon class
//Oops, we need a point class to support our polygon, lets borrow
//the CPoint example
struct CPoint {
  int x;
  int y;
};

//we have to get CPoint working with boost polygon to make our polygon
//that uses CPoint working with boost polygon
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

//I'm lazy and use the stl everywhere to avoid writing my own classes
//my toy polygon is a std::list<CPoint>
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
	t.insert(t.end(), input_begin, input_end);
	return t;
      }

    };
  } }

//now there's nothing left to do but test that our polygon
//works with library interfaces
int main() {
  test_polygon<CPolygon>(); //woot!
  return 0;
}
