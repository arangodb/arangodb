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

   //set(AVL) vs set(RB)
   launch_tests< set<int, std::less<int>, std::allocator<int>, tree_assoc_options< tree_type<avl_tree> >::type >, set<int> >
      ("set<int>(AVL)", "set<int>(RB)");
   launch_tests< set<string, std::less<string>, std::allocator<string>, tree_assoc_options< tree_type<avl_tree> >::type >, set<string> >
      ("set<string>(AVL)", "set<string>(RB)");

   //set(AVL,sizeopt) vs set(AVL,!sizeopt)
   launch_tests< set<int, std::less<int>, std::allocator<int>, tree_assoc_options< tree_type<avl_tree> >::type >
               , set<int, std::less<int>, std::allocator<int>, tree_assoc_options< tree_type<avl_tree>, optimize_size<false> >::type > >
      ("set<int>(AVL,sizeopt=true)", "set<int>(AVL,sizeopt=false)");
   launch_tests< set<string, std::less<string>, std::allocator<string>, tree_assoc_options< tree_type<avl_tree> >::type >
               , set<string, std::less<string>, std::allocator<string>, tree_assoc_options< tree_type<avl_tree>, optimize_size<false> >::type > >
      ("set<string>(AVL,sizeopt=true)", "set<string>(AVL,sizeopt=false)");

   return 0;
}
