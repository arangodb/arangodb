//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//[doc_extended_allocators
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/list.hpp>
#include <boost/container/set.hpp>

//"allocator" is a general purpose allocator that can reallocate
//memory, something useful for vector and flat associative containers
#include <boost/container/allocator.hpp>

//"adaptive_pool" is a node allocator, specially suited for
//node-based containers
#include <boost/container/adaptive_pool.hpp>

int main ()
{
   using namespace boost::container;

   //A vector that can reallocate memory to implement faster insertions
   vector<int, allocator<int> > extended_alloc_vector;

   //A flat set that can reallocate memory to implement faster insertions
   flat_set<int, std::less<int>, allocator<int> > extended_alloc_flat_set;

   //A list that can manages nodes to implement faster
   //range insertions and deletions
   list<int, adaptive_pool<int> > extended_alloc_list;

   //A set that can recycle nodes to implement faster
   //range insertions and deletions
   set<int, std::less<int>, adaptive_pool<int> > extended_alloc_set;

   //Now user them as always
   extended_alloc_vector.push_back(0);
   extended_alloc_flat_set.insert(0);
   extended_alloc_list.push_back(0);
   extended_alloc_set.insert(0);

   //...
   return 0;
}
//]
