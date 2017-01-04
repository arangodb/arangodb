/*
Copyright 2008 Intel Corporation

Use, modification and distribution are subject to the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt).
*/
#include <boost/polygon/polygon.hpp>
#include <cassert>
namespace gtl = boost::polygon;
using namespace boost::polygon::operators;

//lets make the body of main from point_usage.cpp
//a generic function parameterized by point type
template <typename Point>
void test_point() {
  //constructing a gtl point
  int x = 10;
  int y = 20;
  //Point pt(x, y);
  Point pt = gtl::construct<Point>(x, y);
  assert(gtl::x(pt) == 10);
  assert(gtl::y(pt) == 20);
    
  //a quick primer in isotropic point access
  typedef gtl::orientation_2d O;
  using gtl::HORIZONTAL;
  using gtl::VERTICAL;
  O o = HORIZONTAL;
  assert(gtl::x(pt) == gtl::get(pt, o));
    
  o = o.get_perpendicular();
  assert(o == VERTICAL);
  assert(gtl::y(pt) == gtl::get(pt, o));
    
  gtl::set(pt, o, 30);
  assert(gtl::y(pt) == 30);
    
  //using some of the library functions
  //Point pt2(10, 30);
  Point pt2 = gtl::construct<Point>(10, 30);
  assert(gtl::equivalence(pt, pt2));
    
  gtl::transformation<int> tr(gtl::axis_transformation::SWAP_XY);
  gtl::transform(pt, tr);
  assert(gtl::equivalence(pt, gtl::construct<Point>(30, 10)));
    
  gtl::transformation<int> tr2 = tr.inverse();
  assert(tr == tr2); //SWAP_XY is its own inverse transform
    
  gtl::transform(pt, tr2);
  assert(gtl::equivalence(pt, pt2)); //the two points are equal again
    
  gtl::move(pt, o, 10); //move pt 10 units in y
  assert(gtl::euclidean_distance(pt, pt2) == 10.0f);
    
  gtl::move(pt, o.get_perpendicular(), 10); //move pt 10 units in x
  assert(gtl::manhattan_distance(pt, pt2) == 20);
}
    
//Now lets declare our own point type
//Bjarne says that if a class doesn't maintain an
//invariant just use a struct.
struct CPoint {
  int x;
  int y;
};
    
//There, nice a simple...but wait, it doesn't do anything
//how do we use it to do all the things a point needs to do?
    
    
//First we register it as a point with boost polygon
namespace boost { namespace polygon {
    template <>
    struct geometry_concept<CPoint> { typedef point_concept type; };
 
    
    //Then we specialize the gtl point traits for our point type
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
    
//Now lets see if the CPoint works with the library functions
int main() {
  test_point<CPoint>(); //yay! All your testing is done for you.
  return 0;
}
    
//Now you know how to map a user type to the library point concept
//and how to write a generic function parameterized by point type
//using the library interfaces to access it.
