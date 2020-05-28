//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include "boost/container/set.hpp"
#include "boost/container/flat_set.hpp"
#include "bench_set.hpp"

int main()
{
   using namespace boost::container;

   fill_range_ints();
   fill_range_strings();

   //flat_set vs set
   launch_tests< flat_multiset<int> , multiset<int> >
      ("flat_multiset<int>", "multiset<int>");
   launch_tests< flat_multiset<string> , multiset<string> >
      ("flat_multiset<string>", "multiset<string>");

   return 0;
}
