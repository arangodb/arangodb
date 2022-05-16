// Boost.Geometry
// QuickBook Example

// Copyright (c) 2020, Aditya Mohan

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[cross_product
//` Calculate the cross product of two points


#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/arithmetic/cross_product.hpp>

namespace bg = boost::geometry; /*< Convenient namespace alias >*/

int main()
{
     //Example 1 2D Vector
     
     bg::model::point<double, 2, bg::cs::cartesian> p1(7.0, 2.0);
     bg::model::point<double, 2, bg::cs::cartesian> p2(4.0, 5.0);
           
     bg::model::point<double, 2, bg::cs::cartesian> r1;
       
     r1 = bg::cross_product(p1,p2);

     std::cout << "Cross Product 1: "<< r1.get<0>() << std::endl;  //Note that the second point (r1.get<1>) would be undefined in this case
        
     //Example 2 - 3D Vector 
     
     bg::model::point<double, 3, bg::cs::cartesian> p3(4.0, 6.0, 5.0);
     bg::model::point<double, 3, bg::cs::cartesian> p4(7.0, 2.0, 3.0);
           
     bg::model::point<double, 3, bg::cs::cartesian> r2;
       
     r2 = bg::cross_product(p3,p4);

     std::cout << "Cross Product 2: ("<< r2.get<0>() <<","<< r2.get<1>() <<","<< r2.get<2>() << ")"<< std::endl; 
     
     return 0;
}

//]

//[cross_product_output
/*`
Output:
[pre
Cross Product 1: 27
Cross Product 2: (8,23,-34)
]
*/
//]
