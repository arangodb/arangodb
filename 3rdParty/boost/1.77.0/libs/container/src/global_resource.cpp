//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#define BOOST_CONTAINER_SOURCE
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/container/throw_exception.hpp>
#include <boost/container/detail/dlmalloc.hpp>  //For global lock
#include <boost/container/detail/singleton.hpp>

#include <cstddef>
#include <new>

namespace boost {
namespace container {
namespace pmr {

class new_delete_resource_imp
   : public memory_resource
{
   public:

   ~new_delete_resource_imp() BOOST_OVERRIDE
   {}

   void* do_allocate(std::size_t bytes, std::size_t alignment) BOOST_OVERRIDE
   {  (void)bytes; (void)alignment; return new char[bytes];  }

   void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) BOOST_OVERRIDE
   {  (void)bytes; (void)alignment; delete[]((char*)p);  }

   bool do_is_equal(const memory_resource& other) const BOOST_NOEXCEPT BOOST_OVERRIDE
   {  return &other == this;   }
};

struct null_memory_resource_imp
   : public memory_resource
{
   public:

   ~null_memory_resource_imp() BOOST_OVERRIDE
   {}

   void* do_allocate(std::size_t bytes, std::size_t alignment) BOOST_OVERRIDE
   {
      (void)bytes; (void)alignment;
      #if defined(BOOST_CONTAINER_USER_DEFINED_THROW_CALLBACKS) || defined(BOOST_NO_EXCEPTIONS)
      throw_bad_alloc();
      #else
      throw std::bad_alloc();
      #endif

      return 0;
   }

   void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) BOOST_OVERRIDE
   {  (void)p;  (void)bytes; (void)alignment;  }

   bool do_is_equal(const memory_resource& other) const BOOST_NOEXCEPT BOOST_OVERRIDE
   {  return &other == this;   }
};

BOOST_CONTAINER_DECL memory_resource* new_delete_resource() BOOST_NOEXCEPT
{
   return &boost::container::dtl::singleton_default<new_delete_resource_imp>::instance();
}

BOOST_CONTAINER_DECL memory_resource* null_memory_resource() BOOST_NOEXCEPT
{
   return &boost::container::dtl::singleton_default<null_memory_resource_imp>::instance();
}

static memory_resource *default_memory_resource =
   &boost::container::dtl::singleton_default<new_delete_resource_imp>::instance();

BOOST_CONTAINER_DECL memory_resource* set_default_resource(memory_resource* r) BOOST_NOEXCEPT
{
   //TO-DO: synchronizes-with part using atomics
   if(dlmalloc_global_sync_lock()){
      memory_resource *previous = default_memory_resource;
      if(!previous){
         //function called before main, default_memory_resource is not initialized yet
         previous = new_delete_resource();
      }
      default_memory_resource = r ? r : new_delete_resource();
      dlmalloc_global_sync_unlock();
      return previous;
   }
   else{
      return new_delete_resource();
   }
}

BOOST_CONTAINER_DECL memory_resource* get_default_resource() BOOST_NOEXCEPT
{
   //TO-DO: synchronizes-with part using atomics
   if(dlmalloc_global_sync_lock()){
      memory_resource *current = default_memory_resource;
      if(!current){
         //function called before main, default_memory_resource is not initialized yet
         current = new_delete_resource();
      }
      dlmalloc_global_sync_unlock();
      return current;
   }
   else{
      return new_delete_resource();
   }
}

}  //namespace pmr {
}  //namespace container {
}  //namespace boost {
