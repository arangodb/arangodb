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

   //set vs std::set
   launch_tests< set<int> , std::set<int> >
      ("set<int>", "std::set<int>");
   launch_tests< set<string> , std::set<string> >
      ("set<string>", "std::set<string>");

   //set(sizeopt) vs set(!sizeopt)
   launch_tests< set<int>, set<int, std::less<int>, std::allocator<int>, tree_assoc_options< optimize_size<false> >::type > >
      ("set<int>(sizeopt=true)", "set<int>(sizeopt=false)");
   launch_tests< set<string>, set<string, std::less<string>, std::allocator<string>, tree_assoc_options< optimize_size<false> >::type > >
      ("set<string>(sizeopt=true)", "set<string>(sizeopt=false)");

   return 0;
}
