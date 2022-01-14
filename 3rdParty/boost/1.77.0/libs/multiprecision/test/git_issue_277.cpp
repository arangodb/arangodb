///////////////////////////////////////////////////////////////////////////////
//  Copyright 2019 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_int.hpp>
#include "test.hpp"

template <class I>
void test()
{
   I val(1);
   val <<= 512;
   I t(val);
   ++t;
   BOOST_CHECK_EQUAL(t - val, 1);
   --t;
   BOOST_CHECK_EQUAL(t, val);
   --t;
   BOOST_CHECK_EQUAL(val - t, 1);

   val = -val;
   t   = val;
   --t;
   BOOST_CHECK_EQUAL(t - val, -1);
   ++t;
   BOOST_CHECK_EQUAL(t, val);
   ++t;
   BOOST_CHECK_EQUAL(val - t, -1);
}

int main()
{
   test<boost::multiprecision::cpp_int>();
   test<boost::multiprecision::int1024_t>();
   test<boost::multiprecision::checked_int1024_t>();
   return boost::report_errors();
}


