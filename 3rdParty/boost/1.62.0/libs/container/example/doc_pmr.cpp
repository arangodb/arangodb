//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_pmr_ShoppingList_hpp
//ShoppingList.hpp
#include <boost/container/pmr/vector.hpp>
#include <boost/container/pmr/string.hpp>

class ShoppingList
{
   // A vector of strings using polymorphic allocators. Every element
   // of the vector will use the same allocator as the vector itself.
   boost::container::pmr::vector_of
      <boost::container::pmr::string>::type m_strvec;
   //Alternatively in compilers that support template aliases:
   //    boost::container::pmr::vector<boost::container::pmr::string> m_strvec;
   public:

   // This makes uses_allocator<ShoppingList, memory_resource*>::value true
   typedef boost::container::pmr::memory_resource* allocator_type;

   // If the allocator is not specified, "m_strvec" uses pmr::get_default_resource().
   explicit ShoppingList(allocator_type alloc = 0)
      : m_strvec(alloc) {}

   // Copy constructor. As allocator is not specified,
   // "m_strvec" uses pmr::get_default_resource().
   ShoppingList(const ShoppingList& other)
      : m_strvec(other.m_strvec) {}

   // Copy construct using the given memory_resource.
   ShoppingList(const ShoppingList& other, allocator_type a)
      : m_strvec(other.m_strvec, a) {}

   allocator_type get_allocator() const
   { return m_strvec.get_allocator().resource(); }

   void add_item(const char *item)
   { m_strvec.emplace_back(item); }

   //...
};

//]]

//[doc_pmr_main_cpp

//=#include "ShoppingList.hpp"
#include <cassert>
#include <boost/container/pmr/list.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>

void processShoppingList(const ShoppingList&)
{  /**/   }

int main()
{
   using namespace boost::container;
   //All memory needed by folder and its contained objects will
   //be allocated from the default memory resource (usually new/delete) 
   pmr::list_of<ShoppingList>::type folder; // Default allocator resource
   //Alternatively in compilers that support template aliases:
   //    boost::container::pmr::list<ShoppingList> folder;
   {
      char buffer[1024];
      pmr::monotonic_buffer_resource buf_rsrc(&buffer, 1024);

      //All memory needed by temporaryShoppingList will be allocated
      //from the local buffer (speeds up "processShoppingList")
      ShoppingList temporaryShoppingList(&buf_rsrc);
      assert(&buf_rsrc == temporaryShoppingList.get_allocator());

      //list nodes, and strings "salt" and "pepper" will be allocated
      //in the stack thanks to "monotonic_buffer_resource".
      temporaryShoppingList.add_item("salt");
      temporaryShoppingList.add_item("pepper");
      //...

      //All modifications and additions to "temporaryShoppingList"
      //will use memory from "buffer" until it's exhausted.
      processShoppingList(temporaryShoppingList);

      //Processing done, now insert it in "folder",
      //which uses the default memory resource
      folder.push_back(temporaryShoppingList);
      assert(pmr::get_default_resource() == folder.back().get_allocator());
      //temporaryShoppingList, buf_rsrc, and buffer go out of scope
   }
   return 0;
}

//]
