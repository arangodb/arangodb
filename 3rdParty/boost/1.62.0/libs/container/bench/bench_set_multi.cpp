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
#include <set>

#include "bench_set.hpp"

int main()
{
   using namespace boost::container;

   fill_range_ints();
   fill_range_strings();

   //multiset vs std::multiset
   launch_tests< multiset<int> , std::multiset<int> >
      ("multiset<int>", "std::multiset<int>");
   launch_tests< multiset<string> , std::multiset<string> >
      ("multiset<string>", "std::multiset<string>");

   return 0;
}
