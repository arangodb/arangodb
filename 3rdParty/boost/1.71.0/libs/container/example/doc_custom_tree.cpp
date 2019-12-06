//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
//[doc_custom_tree
#include <boost/container/set.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   //First define several options
   //

   //This option specifies an AVL tree based associative container
   typedef tree_assoc_options< tree_type<avl_tree> >::type AVLTree;

   //This option specifies an AVL tree based associative container
   //disabling node size optimization.
   typedef tree_assoc_options< tree_type<avl_tree>
                             , optimize_size<false> >::type AVLTreeNoSizeOpt;

   //This option specifies an Splay tree based associative container
   typedef tree_assoc_options< tree_type<splay_tree> >::type SplayTree;

   //Now define new tree-based associative containers
   //

   //AVLTree based set container
   typedef set<int, std::less<int>, std::allocator<int>, AVLTree> AvlSet;

   //AVLTree based set container without size optimization
   typedef set<int, std::less<int>, std::allocator<int>, AVLTreeNoSizeOpt> AvlSetNoSizeOpt;

   //Splay tree based multiset container
   typedef multiset<int, std::less<int>, std::allocator<int>, SplayTree> SplayMultiset;

   //Use them
   //
   AvlSet avl_set;
   avl_set.insert(0);
   assert(avl_set.find(0) != avl_set.end());

   AvlSetNoSizeOpt avl_set_no_szopt;
   avl_set_no_szopt.insert(1);
   avl_set_no_szopt.insert(1);
   assert(avl_set_no_szopt.count(1) == 1);

   SplayMultiset splay_mset;
   splay_mset.insert(2);
   splay_mset.insert(2);
   assert(splay_mset.count(2) == 2);
   return 0;
}
//]
#include <boost/container/detail/config_end.hpp>
