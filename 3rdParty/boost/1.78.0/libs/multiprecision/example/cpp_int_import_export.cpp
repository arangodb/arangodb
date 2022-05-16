///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_int.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <iterator>

//[IE1

/*`
In this simple example, we'll import/export the bits of a cpp_int
to a vector of 8-bit unsigned values:
*/
/*=
#include <boost/multiprecision/cpp_int.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <iterator>
*/

int main()
{
   using boost::multiprecision::cpp_int;
   // Create a cpp_int with just a couple of bits set:
   cpp_int i;
   bit_set(i, 5000); // set the 5000'th bit
   bit_set(i, 200);
   bit_set(i, 50);
   // export into 8-bit unsigned values, most significant bit first:
   std::vector<unsigned char> v;
   export_bits(i, std::back_inserter(v), 8);
   // import back again, and check for equality:
   cpp_int j;
   import_bits(j, v.begin(), v.end());
   BOOST_ASSERT(i == j);
}

//]
