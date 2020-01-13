//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/no_exceptions_support.hpp>

#include "derived_from_memory_resource.hpp"

#include <cstdlib>
#include <new>

using namespace boost::container;
using namespace boost::container::pmr;

#ifdef BOOST_MSVC
#pragma warning (push)
#pragma warning (disable : 4290)
#endif

#if __cplusplus >= 201103L
#define BOOST_CONTAINER_NEW_EXCEPTION_SPECIFIER
#define BOOST_CONTAINER_DELETE_EXCEPTION_SPECIFIER noexcept
#else
#define BOOST_CONTAINER_NEW_EXCEPTION_SPECIFIER    throw(std::bad_alloc)
#define BOOST_CONTAINER_DELETE_EXCEPTION_SPECIFIER throw()
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 50000)
#pragma GCC diagnostic ignored "-Wsized-deallocation"
#endif

//ASAN does not support operator new overloading
#ifndef BOOST_CONTAINER_ASAN

std::size_t allocation_count = 0;

void* operator new[](std::size_t count) BOOST_CONTAINER_NEW_EXCEPTION_SPECIFIER
{
   ++allocation_count;
   return std::malloc(count);
}

void operator delete[](void *p) BOOST_CONTAINER_DELETE_EXCEPTION_SPECIFIER
{
   --allocation_count;
   return std::free(p);
}

#endif   //BOOST_CONTAINER_ASAN

#ifdef BOOST_MSVC
#pragma warning (pop)
#endif

#ifndef BOOST_CONTAINER_ASAN

void test_new_delete_resource()
{
   //Make sure new_delete_resource calls new[]/delete[]
   std::size_t memcount = allocation_count;
   memory_resource *mr = new_delete_resource();
   //each time should return the same pointer
   BOOST_TEST(mr == new_delete_resource());
   #if !defined(BOOST_CONTAINER_DYNAMIC_LINKING)  //No new delete replacement possible new_delete is a DLL
   BOOST_TEST(memcount == allocation_count);
   #endif
   void *addr = mr->allocate(16, 1);
   #if !defined(BOOST_CONTAINER_DYNAMIC_LINKING)  //No new delete replacement possible new_delete is a DLL
   BOOST_TEST((allocation_count - memcount) == 1);
   #endif
   mr->deallocate(addr, 16, 1);
   BOOST_TEST(memcount == allocation_count);
}

#endif   //BOOST_CONTAINER_ASAN

void test_null_memory_resource()
{
   //Make sure it throw or returns null
   memory_resource *mr = null_memory_resource();
   #if !defined(BOOST_NO_EXCEPTIONS)
   bool bad_allocexception_thrown = false;
   try{
      mr->allocate(1, 1);
   }
   catch(std::bad_alloc&) {
      bad_allocexception_thrown = true;
   }
   catch(...) {
   }
   BOOST_TEST(bad_allocexception_thrown == true);
   #else
   BOOST_TEST(0 == mr->allocate(1, 1));
   #endif
}

void test_default_resource()
{
   //Default resource must be new/delete before set_default_resource
   BOOST_TEST(get_default_resource() == new_delete_resource());
   //Set default resource and obtain previous
   derived_from_memory_resource d;
   memory_resource *prev_default = set_default_resource(&d);
   BOOST_TEST(get_default_resource() == &d);
   //Set default resource with null, which should be new/delete
   prev_default = set_default_resource(0);
   BOOST_TEST(prev_default == &d);
   BOOST_TEST(get_default_resource() == new_delete_resource());
}

int main()
{
   #ifndef BOOST_CONTAINER_ASAN
   test_new_delete_resource();
   #endif
   test_null_memory_resource();
   test_default_resource();
   return ::boost::report_errors();
}
