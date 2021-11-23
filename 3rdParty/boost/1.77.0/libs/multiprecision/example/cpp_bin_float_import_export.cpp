///////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <iterator>

// Contains Quickbook snippets in comments.

//[IE2

/*`
Importing or exporting cpp_bin_float is similar, but we must proceed via an intermediate integer:
*/
/*=
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <iterator>
*/

int main()
{
   using boost::multiprecision::cpp_bin_float_100;
   using boost::multiprecision::cpp_int;
   // Create a cpp_bin_float to import/export:
   cpp_bin_float_100 f(1);
   f /= 3;
   // export into 8-bit unsigned values, most significant bit first:
   std::vector<unsigned char> v;
   export_bits(cpp_int(f.backend().bits()), std::back_inserter(v), 8);
   // Grab the exponent as well:
   int e = f.backend().exponent();
   // Import back again, and check for equality, we have to proceed via
   // an intermediate integer:
   cpp_int i;
   import_bits(i, v.begin(), v.end());
   cpp_bin_float_100 g(i);
   g.backend().exponent() = e;
   BOOST_ASSERT(f == g);
}

//]

