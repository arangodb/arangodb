///////////////////////////////////////////////////////////////////////////////
//  Copyright 2020 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/mpfr.hpp>
#include <boost/math/special_functions/next.hpp>
#include "test.hpp"

using namespace boost::multiprecision;

int main()
{
   mpfr_float_50 half = 0.5;
   mpfr_float_50 under_half = boost::math::float_prior(half);
   BOOST_CHECK_NE(half, under_half);
   int e1, e2;
   mpfr_float_50 norm1, norm2;
   norm1 = frexp(half, &e1);
   norm2 = frexp(under_half, &e2);
   BOOST_CHECK_EQUAL(norm1, half);
   BOOST_CHECK_EQUAL(e1, 0);
   BOOST_CHECK_GT(1, norm2);
   BOOST_CHECK_LE(0.5, norm2);
   BOOST_CHECK_EQUAL(e2, -1);
   return boost::report_errors();
}
