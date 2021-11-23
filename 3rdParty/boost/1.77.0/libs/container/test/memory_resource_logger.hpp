//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_MEMORY_RESOURCE_TESTER_HPP
#define BOOST_CONTAINER_TEST_MEMORY_RESOURCE_TESTER_HPP

#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/throw_exception.hpp>
#include <cstdlib>

class memory_resource_logger
   : public boost::container::pmr::memory_resource
{
   public:
   struct allocation_info
   {
      char *address;
      std::size_t bytes;
      std::size_t alignment;
   };

   boost::container::vector<allocation_info> m_info;
   unsigned m_mismatches;

   explicit memory_resource_logger()
      : m_info()
      , m_mismatches()
   {}

   virtual ~memory_resource_logger()
   {  this->reset();  }

   virtual void* do_allocate(std::size_t bytes, std::size_t alignment)
   {
      char *addr =(char*)std::malloc(bytes);
      if(!addr){
         boost::container::throw_bad_alloc();
      }
      allocation_info info;
      info.address   = addr;
      info.bytes     = bytes;
      info.alignment = alignment;
      m_info.push_back(info);
      return addr;
   }

   virtual void do_deallocate(void* p, std::size_t bytes, std::size_t alignment)
   {
      std::size_t i = 0, max = m_info.size();
      while(i != max && m_info[i].address != p){
         ++i;
      }
      if(i == max){
         ++m_mismatches;
      }
      else{
         const allocation_info &info = m_info[i];
         m_mismatches += info.bytes != bytes || info.alignment != alignment;
         std::free(p);
         m_info.erase(m_info.nth(i));
      }
   }

   virtual bool do_is_equal(const boost::container::pmr::memory_resource& other) const BOOST_NOEXCEPT
   {
      return static_cast<const memory_resource *>(this) == &other;
   }

   void reset()
   {
      while(!m_info.empty()){
         std::free(m_info.back().address);
         m_info.pop_back();
      }
      m_mismatches = 0u;
   }
};

#endif   //#ifndef BOOST_CONTAINER_TEST_MEMORY_RESOURCE_TESTER_HPP
