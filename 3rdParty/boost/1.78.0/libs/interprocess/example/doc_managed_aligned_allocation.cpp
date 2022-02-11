//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_managed_aligned_allocation
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
   managed_shared_memory managed_shm(create_only, test::get_process_id_name(), 65536);
   #else
   //->
   managed_shared_memory managed_shm(create_only, "MySharedMemory", 65536);
   //<-
   #endif
   //->

   const std::size_t Alignment = 128;

   //Allocate 100 bytes aligned to Alignment from segment, throwing version
   void *ptr = managed_shm.allocate_aligned(100, Alignment);

   //Check alignment
   assert(std::size_t(static_cast<char*>(ptr)-static_cast<char*>(0)) % Alignment == 0);

   //Deallocate it
   managed_shm.deallocate(ptr);

   //Non throwing version
   ptr = managed_shm.allocate_aligned(100, Alignment, std::nothrow);

   //Check alignment
   assert(std::size_t(static_cast<char*>(ptr)-static_cast<char*>(0)) % Alignment == 0);

   //Deallocate it
   managed_shm.deallocate(ptr);

   //If we want to efficiently allocate aligned blocks of memory
   //use managed_shared_memory::PayloadPerAllocation value
   assert(Alignment > managed_shared_memory::PayloadPerAllocation);

   //This allocation will maximize the size of the aligned memory
   //and will increase the possibility of finding more aligned memory
   ptr = managed_shm.allocate_aligned
      (3u*Alignment - managed_shared_memory::PayloadPerAllocation, Alignment);

   //Check alignment
   assert(std::size_t(static_cast<char*>(ptr)-static_cast<char*>(0)) % Alignment == 0);

   //Deallocate it
   managed_shm.deallocate(ptr);

   return 0;
}
//]

