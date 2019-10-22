///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_int.hpp>

#include "eigen.hpp"

int main()
{
   using namespace boost::multiprecision;
   test_integer_type<int>();
   test_integer_type<boost::multiprecision::int256_t>();
   test_integer_type<boost::multiprecision::cpp_int>();
   return 0;
}
