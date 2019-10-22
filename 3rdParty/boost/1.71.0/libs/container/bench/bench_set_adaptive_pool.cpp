//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//Enable checks in debug mode
#ifndef NDEBUG
#define BOOST_CONTAINER_ADAPTIVE_NODE_POOL_CHECK_INVARIANTS
#endif

#include "bench_set.hpp"
#include <boost/container/set.hpp>
#include <boost/container/allocator.hpp>
#include <boost/container/adaptive_pool.hpp>

int main()
{
   using namespace boost::container;

   fill_range_ints();
   fill_range_strings();

   //set<..., adaptive_pool> vs. set
   launch_tests< set<int, std::less<int>, private_adaptive_pool<int> >, set<int> >
      ("set<int, ..., private_adaptive_pool<int>", "set<int>");
   launch_tests< set<string, std::less<string>, private_adaptive_pool<string> >, set<string> >
      ("set<string, ..., private_adaptive_pool<string>", "set<string>");

   return 0;
}
