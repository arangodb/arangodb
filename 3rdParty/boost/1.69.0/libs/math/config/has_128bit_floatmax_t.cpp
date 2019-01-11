//  Copyright John Maddock 2014.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/cstdfloat.hpp>
#include <boost/static_assert.hpp>

#ifndef BOOST_FLOAT128_C
#error "There is no 128 bit floating point type"
#endif

BOOST_STATIC_ASSERT(sizeof(boost::floatmax_t) * CHAR_BIT == 128);

int main()
{
   return 0;
}

