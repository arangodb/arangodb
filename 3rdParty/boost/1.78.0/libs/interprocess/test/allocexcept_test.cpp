//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/list.hpp>
#include <iostream>
#include <functional>
#include "print_container.hpp"
#include <string>
#include "get_process_id_name.hpp"

struct InstanceCounter
{
   InstanceCounter(){++counter;}
   InstanceCounter(const InstanceCounter&){++counter;}
   InstanceCounter & operator=(const InstanceCounter&){  return *this;  }
  ~InstanceCounter(){--counter;}
   static std::size_t counter;
};

std::size_t InstanceCounter::counter = 0;

using namespace boost::interprocess;


int main ()
{
   const int memsize = 16384;
   const char *const shMemName = test::get_process_id_name();

   BOOST_TRY{
      shared_memory_object::remove(shMemName);

      //Named allocate capable shared mem allocator
      managed_shared_memory segment(create_only, shMemName, memsize);

      //STL compatible allocator object, uses allocate(), deallocate() functions
      typedef allocator<InstanceCounter,
                        managed_shared_memory::segment_manager>
         inst_allocator_t;
      const inst_allocator_t myallocator (segment.get_segment_manager());

      typedef vector<InstanceCounter, inst_allocator_t> MyVect;

      //We'll provoke an exception, let's see if exception handling works
      BOOST_TRY{
         //Fill vector until there is no more memory
         MyVect myvec(myallocator);
         std::size_t i;
         for(i = 0; true; ++i){
            myvec.push_back(InstanceCounter());
         }
      }
      BOOST_CATCH(boost::interprocess::bad_alloc &){
         if(InstanceCounter::counter != 0)
            return 1;
      } BOOST_CATCH_END

      //We'll provoke an exception, let's see if exception handling works
      BOOST_TRY{
         //Fill vector at the beginning until there is no more memory
         MyVect myvec(myallocator);
         std::size_t i;
         InstanceCounter ic;
         for(i = 0; true; ++i){
            myvec.insert(myvec.begin(), i, ic);
         }
      }
      BOOST_CATCH(boost::interprocess::bad_alloc &){
         if(InstanceCounter::counter != 0)
            return 1;
      }
      BOOST_CATCH(std::length_error &){
         if(InstanceCounter::counter != 0)
            return 1;
      } BOOST_CATCH_END
   }
   BOOST_CATCH(...){
      shared_memory_object::remove(shMemName);
      BOOST_RETHROW
   }
   BOOST_CATCH_END

   shared_memory_object::remove(shMemName);
   return 0;
}

