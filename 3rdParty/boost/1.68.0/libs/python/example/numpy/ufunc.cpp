// Copyright Ankit Daftery 2011-2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

/**
 *  @brief An example to demonstrate use of universal functions or ufuncs
 *        
 *
 * @todo Calling the overloaded () operator is in a roundabout manner, find a simpler way
 *       None of the methods like np::add, np::multiply etc are supported as yet
 */

#include <boost/python/numpy.hpp>
#include <iostream>

namespace p = boost::python;
namespace np = boost::python::numpy;


// Create the structs necessary to implement the ufuncs 
// The typedefs *must* be made

struct UnarySquare 
{
  typedef double argument_type;
  typedef double result_type;

  double operator()(double r) const { return r * r;}
};

struct BinarySquare
{
  typedef double first_argument_type;
  typedef double second_argument_type;
  typedef double result_type;

  double operator()(double a,double b) const { return (a*a + b*b) ; }
};

int main(int argc, char **argv)
{
  // Initialize the Python runtime.
  Py_Initialize();
  // Initialize NumPy
  np::initialize();
  // Expose the struct UnarySquare to Python as a class, and let ud be the class object
  p::object ud = p::class_<UnarySquare, boost::shared_ptr<UnarySquare> >("UnarySquare")
    .def("__call__", np::unary_ufunc<UnarySquare>::make());
  // Let inst be an instance of the class ud
  p::object inst = ud();
  // Use the "__call__" method to call the overloaded () operator and print the value
  std::cout << "Square of unary scalar 1.0 is " << p::extract <char const * > (p::str(inst.attr("__call__")(1.0))) << std::endl ; 
  // Create an array in C++
  int arr[] = {1,2,3,4} ; 
  // ..and use it to create the ndarray in Python
  np::ndarray demo_array = np::from_data(arr, np::dtype::get_builtin<int>() , p::make_tuple(4), p::make_tuple(4), p::object());
  // Print out the demo array
  std::cout << "Demo array is " << p::extract <char const * > (p::str(demo_array)) << std::endl ; 
  // Call the "__call__" method to perform the operation and assign the value to result_array
  p::object result_array = inst.attr("__call__")(demo_array) ;
  // Print the resultant array
  std::cout << "Square of demo array is " << p::extract <char const * > (p::str(result_array)) << std::endl ; 
  // Lets try the same with a list
  p::list li ;
  li.append(3);
  li.append(7);
  // Print out the demo list
  std::cout << "Demo list is " << p::extract <char const * > (p::str(li)) << std::endl ; 
  // Call the ufunc for the list
  result_array = inst.attr("__call__")(li) ;
  // And print the list out
  std::cout << "Square of demo list is " << p::extract <char const * > (p::str(result_array)) << std::endl ; 
  // Now lets try Binary ufuncs
  // Expose the struct BinarySquare to Python as a class, and let ud be the class object
  ud = p::class_<BinarySquare, boost::shared_ptr<BinarySquare> >("BinarySquare")
    .def("__call__", np::binary_ufunc<BinarySquare>::make());
  // Again initialise inst as an instance of the class ud
  inst = ud();
  // Print the two input listsPrint the two input lists
  std::cout << "The two input list for binary ufunc are " << std::endl << p::extract <char const * > (p::str(demo_array)) << std::endl << p::extract <char const * > (p::str(demo_array)) << std::endl ; 
  // Call the binary ufunc taking demo_array as both inputs
  result_array = inst.attr("__call__")(demo_array,demo_array) ;
  std::cout << "Square of list with binary ufunc is " << p::extract <char const * > (p::str(result_array)) << std::endl ; 
}
  
