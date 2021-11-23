//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/interprocess/detail/config_begin.hpp>
//[doc_managed_allocation_command
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cassert>
//<-
#include "../test/get_process_id_name.hpp"
//->

int main()
{
   using namespace boost::interprocess;

   //Remove shared memory on construction and destruction
   struct shm_remove
   {
   //<-
   #if 1
      shm_remove() { shared_memory_object::remove(test::get_process_id_name()); }
      ~shm_remove(){ shared_memory_object::remove(test::get_process_id_name()); }
   #else
   //->
      shm_remove() { shared_memory_object::remove("MySharedMemory"); }
      ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
   //<-
   #endif
   //->
   } remover;
   //<-
   (void)remover;
   //->

   //Managed memory segment that allocates portions of a shared memory
   //segment with the default management algorithm
   //<-
   #if 1
   managed_shared_memory managed_shm(create_only, test::get_process_id_name(), 10000*sizeof(std::size_t));
   #else
   //->
   managed_shared_memory managed_shm(create_only, "MySharedMemory", 10000*sizeof(std::size_t));
   //<-
   #endif
   //->

   //Allocate at least 100 bytes, 1000 bytes if possible
   managed_shared_memory::size_type min_size = 100;
   managed_shared_memory::size_type first_received_size = 1000;
   std::size_t *hint = 0;
   std::size_t *ptr = managed_shm.allocation_command<std::size_t>
      (boost::interprocess::allocate_new, min_size, first_received_size, hint);

   //Received size must be bigger than min_size
   assert(first_received_size >= min_size);

   //Get free memory
   managed_shared_memory::size_type free_memory_after_allocation = managed_shm.get_free_memory();
   //<-
   (void)free_memory_after_allocation;
   //->

   //Now write the data
   for(std::size_t i = 0; i < first_received_size; ++i) ptr[i] = i;

   //Now try to triplicate the buffer. We won't admit an expansion
   //lower to the double of the original buffer.
   //This "should" be successful since no other class is allocating
   //memory from the segment
   min_size = first_received_size*2;
   managed_shared_memory::size_type expanded_size = first_received_size*3;
   std::size_t * ret = managed_shm.allocation_command
      (boost::interprocess::expand_fwd, min_size, expanded_size, ptr);
   //<-
   (void)ret;
   //->
   //Check invariants
   assert(ptr != 0);
   assert(ret == ptr);
   assert(expanded_size >= first_received_size*2);

   //Get free memory and compare
   managed_shared_memory::size_type free_memory_after_expansion = managed_shm.get_free_memory();
   assert(free_memory_after_expansion < free_memory_after_allocation);
   //<-
   (void)free_memory_after_expansion;
   //->

   //Write new values
   for(std::size_t i = first_received_size; i < expanded_size; ++i)  ptr[i] = i;

   //Try to shrink approximately to min_size, but the new size
   //should be smaller than min_size*2.
   //This "should" be successful since no other class is allocating
   //memory from the segment
   managed_shared_memory::size_type shrunk_size = min_size;
   ret = managed_shm.allocation_command
      (boost::interprocess::shrink_in_place, min_size*2, shrunk_size, ptr);

   //Check invariants
   assert(ptr != 0);
   assert(ret == ptr);
   assert(shrunk_size <= min_size*2);
   assert(shrunk_size >= min_size);

   //Get free memory and compare
   managed_shared_memory::size_type free_memory_after_shrinking = managed_shm.get_free_memory();
   assert(free_memory_after_shrinking > free_memory_after_expansion);
   //<-
   (void)free_memory_after_shrinking;
   //->

   //Deallocate the buffer
   managed_shm.deallocate(ptr);
   return 0;
}
//]
#include <boost/interprocess/detail/config_end.hpp>
