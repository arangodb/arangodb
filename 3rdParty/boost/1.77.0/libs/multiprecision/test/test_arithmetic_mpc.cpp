///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/mpc.hpp>
#define TEST_MPC

#include "libs/multiprecision/test/test_arithmetic.hpp"

template <unsigned D>
struct related_type<boost::multiprecision::number<boost::multiprecision::mpc_complex_backend<D> > >
{
   typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<D> > type;
};

int main()
{
   test<boost::multiprecision::mpc_complex_50>();
   return boost::report_errors();
}
