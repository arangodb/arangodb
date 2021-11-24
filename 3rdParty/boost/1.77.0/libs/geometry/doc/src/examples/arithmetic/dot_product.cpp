// Boost.Geometry
// QuickBook Example

// Copyright (c) 2020, Aditya Mohan

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[dot_product
//` Calculate the dot product of two points

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/arithmetic/dot_product.hpp>
#include <boost/geometry/geometries/adapted/boost_array.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

BOOST_GEOMETRY_REGISTER_BOOST_ARRAY_CS(cs::cartesian)


int main()
{            
     double dp1,dp2,dp3,dp4;
     bg::model::point<double, 3, bg::cs::cartesian> point1(1.0, 2.0, 3.0);
     bg::model::point<double, 3, bg::cs::cartesian> point2(4.0, 5.0, 6.0);

     //Example 1
     dp1 = bg::dot_product(point1, point2);

     std::cout << "Dot Product 1: "<< dp1 << std::endl; 

     bg::model::point<double, 2, bg::cs::cartesian> point3(3.0, 2.0);
     bg::model::point<double, 2, bg::cs::cartesian> point4(4.0, 7.0);

     //Example 2	
     dp2 = bg::dot_product(point3, point4);

     std::cout << "Dot Product 2: "<< dp2 << std::endl; 

     boost::array<double, 2> a =  {1, 2};
     boost::array<double, 2> b =  {2, 3};

     //Example 3
     dp3 = bg::dot_product(a, b);

     std::cout << "Dot Product 3: "<< dp3 << std::endl; 

     return 0;

}

//]

//[dot_product_output
/*`
Output:
[pre
Dot Product 1: 32
Dot Product 2: 26
Dot Product 3: 8
]
*/
//]
