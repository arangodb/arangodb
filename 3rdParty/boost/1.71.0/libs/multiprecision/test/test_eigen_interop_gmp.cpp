///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/gmp.hpp>

#include "eigen.hpp"

int main()
{
   using namespace boost::multiprecision;
   test_integer_type<boost::multiprecision::mpz_int>();
   test_integer_type<boost::multiprecision::mpq_rational>();
   return 0;
}
