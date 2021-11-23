///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_dec_float.hpp>

#include "eigen.hpp"

template <>
struct related_number<boost::multiprecision::cpp_dec_float_100>
{
   typedef boost::multiprecision::cpp_dec_float_50 type;
};

int main()
{
   using namespace boost::multiprecision;
   test_float_type<double>();
   test_float_type<boost::multiprecision::cpp_dec_float_100>();
   return 0;
}
