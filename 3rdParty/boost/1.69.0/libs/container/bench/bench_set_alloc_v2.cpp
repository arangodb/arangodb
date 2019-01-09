//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include "bench_set.hpp"
#include <boost/container/set.hpp>
#include <boost/container/allocator.hpp>

int main()
{
   using namespace boost::container;

   fill_range_ints();
   fill_range_strings();

   //set<..., version_2> vs. set
   launch_tests< set<int, std::less<int>, allocator<int> >, set<int> >
      ("set<int, ..., allocator<int>", "set<int>");
   launch_tests< set<string, std::less<string>, allocator<string> >, set<string> >
      ("set<string, ..., allocator<string>", "set<string>");

   return 0;
}
