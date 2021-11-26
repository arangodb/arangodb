//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////


#ifdef _MSC_VER
#pragma warning (disable:4702)
#endif

#include <vector>
#include <iostream>
#include <cstring>
#include <algorithm>    //std::remove
#include <boost/container/detail/dlmalloc.hpp>

namespace boost { namespace container { namespace test {

static const int NumIt = 200;

enum deallocation_type { DirectDeallocation, InverseDeallocation, MixedDeallocation, EndDeallocationType };

//This test allocates until there is no more memory
//and after that deallocates all in the inverse order

bool test_allocation()
{
   if(!dlmalloc_all_deallocated())
      return false;
   dlmalloc_malloc_check();
   for( deallocation_type t = DirectDeallocation
      ; t != EndDeallocationType
      ; t = (deallocation_type)((int)t + 1)){
      std::vector<void*> buffers;
      //std::size_t free_memory = a.get_free_memory();

      for(int i = 0; i != NumIt; ++i){
         void *ptr = dlmalloc_malloc(i);
         if(!ptr)
            break;
         buffers.push_back(ptr);
      }

      switch(t){
         case DirectDeallocation:
         {
            for(int j = 0, max = (int)buffers.size()
               ;j < max
               ;++j){
               dlmalloc_free(buffers[j]);
            }
         }
         break;
         case InverseDeallocation:
         {
            for(int j = (int)buffers.size()
               ;j--
               ;){
               dlmalloc_free(buffers[j]);
            }
         }
         break;
         case MixedDeallocation:
         {
            for(int j = 0, max = (int)buffers.size()
               ;j < max
               ;++j){
               int pos = (j%4)*((int)buffers.size())/4;
               dlmalloc_free(buffers[pos]);
               buffers.erase(buffers.begin()+pos);
            }
         }
         break;
         default:
         break;
      }
      if(!dlmalloc_all_deallocated())
         return false;
      //bool ok = free_memory == a.get_free_memory() &&
               //a.all_memory_deallocated() && a.check_sanity();
      //if(!ok)  return ok;
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();
}

//This test allocates until there is no more memory
//and after that tries to shrink all the buffers to the
//half of the original size

bool test_allocation_shrink()
{
   dlmalloc_malloc_check();
   std::vector<void*> buffers;

   //Allocate buffers with extra memory
   for(int i = 0; i != NumIt; ++i){
      void *ptr = dlmalloc_malloc(i*2);
      if(!ptr)
         break;
      buffers.push_back(ptr);
   }

   //Now shrink to half
   for(int i = 0, max = (int)buffers.size()
      ;i < max
      ; ++i){
      std::size_t try_received_size = 0;
      void* try_result = dlmalloc_allocation_command
               ( BOOST_CONTAINER_TRY_SHRINK_IN_PLACE, 1, i*2
               , i, &try_received_size, (char*)buffers[i]).first;

      std::size_t received_size = 0;
      void* result = dlmalloc_allocation_command
         ( BOOST_CONTAINER_SHRINK_IN_PLACE, 1, i*2
         , i, &received_size, (char*)buffers[i]).first;

      if(result != try_result)
         return false;

      if(received_size != try_received_size)
         return false;

      if(result){
         if(received_size > std::size_t(i*2)){
            return false;
         }
         if(received_size < std::size_t(i)){
            return false;
         }
      }
   }

   //Deallocate it in non sequential order
   for(int j = 0, max = (int)buffers.size()
      ;j < max
      ;++j){
      int pos = (j%4)*((int)buffers.size())/4;
      dlmalloc_free(buffers[pos]);
      buffers.erase(buffers.begin()+pos);
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();//a.all_memory_deallocated() && a.check_sanity();
}

//This test allocates until there is no more memory
//and after that tries to expand all the buffers to
//avoid the wasted internal fragmentation

bool test_allocation_expand()
{
   dlmalloc_malloc_check();
   std::vector<void*> buffers;

   //Allocate buffers with extra memory
   for(int i = 0; i != NumIt; ++i){
      void *ptr = dlmalloc_malloc(i);
      if(!ptr)
         break;
      buffers.push_back(ptr);
   }

   //Now try to expand to the double of the size
   for(int i = 0, max = (int)buffers.size()
      ;i < max
      ;++i){
      std::size_t received_size = 0;
      std::size_t min_size = i+1;
      std::size_t preferred_size = i*2;
      preferred_size = min_size > preferred_size ? min_size : preferred_size;
      while(dlmalloc_allocation_command
         ( BOOST_CONTAINER_EXPAND_FWD, 1, min_size
         , preferred_size, &received_size, (char*)buffers[i]).first){
         //Check received size is bigger than minimum
         if(received_size < min_size){
            return false;
         }
         //Now, try to expand further
         min_size       = received_size+1;
         preferred_size = min_size*2;
      }
   }

   //Deallocate it in non sequential order
   for(int j = 0, max = (int)buffers.size()
      ;j < max
      ;++j){
      int pos = (j%4)*((int)buffers.size())/4;
      dlmalloc_free(buffers[pos]);
      buffers.erase(buffers.begin()+pos);
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();//a.all_memory_deallocated() && a.check_sanity();
}

//This test allocates until there is no more memory
//and after that tries to expand all the buffers to
//avoid the wasted internal fragmentation
bool test_allocation_shrink_and_expand()
{
   std::vector<void*> buffers;
   std::vector<std::size_t> received_sizes;
   std::vector<bool>        size_reduced;

   //Allocate buffers wand store received sizes
   for(int i = 0; i != NumIt; ++i){
      std::size_t received_size = 0;
      void *ptr = dlmalloc_allocation_command
         (BOOST_CONTAINER_ALLOCATE_NEW, 1, i, i*2, &received_size, 0).first;
      if(!ptr){
         ptr = dlmalloc_allocation_command
            ( BOOST_CONTAINER_ALLOCATE_NEW, 1, 1, i*2, &received_size, 0).first;
         if(!ptr)
            break;
      }
      buffers.push_back(ptr);
      received_sizes.push_back(received_size);
   }

   //Now shrink to half
   for(int i = 0, max = (int)buffers.size()
      ; i < max
      ; ++i){
      std::size_t received_size = 0;
      bool size_reduced_flag;
      if(true == (size_reduced_flag = !!
         dlmalloc_allocation_command
         ( BOOST_CONTAINER_SHRINK_IN_PLACE, 1, received_sizes[i]
         , i, &received_size, (char*)buffers[i]).first)){
         if(received_size > std::size_t(received_sizes[i])){
            return false;
         }
         if(received_size < std::size_t(i)){
            return false;
         }
      }
      size_reduced.push_back(size_reduced_flag);
   }

   //Now try to expand to the original size
   for(int i = 0, max = (int)buffers.size()
      ;i < max
      ;++i){
      if(!size_reduced[i])  continue;
      std::size_t received_size = 0;
      std::size_t request_size =  received_sizes[i];
      if(dlmalloc_allocation_command
         ( BOOST_CONTAINER_EXPAND_FWD, 1, request_size
         , request_size, &received_size, (char*)buffers[i]).first){
         if(received_size != request_size){
            return false;
         }
      }
      else{
         return false;
      }
   }

   //Deallocate it in non sequential order
   for(int j = 0, max = (int)buffers.size()
      ;j < max
      ;++j){
      int pos = (j%4)*((int)buffers.size())/4;
      dlmalloc_free(buffers[pos]);
      buffers.erase(buffers.begin()+pos);
   }

   return 0 != dlmalloc_all_deallocated();//a.all_memory_deallocated() && a.check_sanity();
}

//This test allocates until there is no more memory
//and after that deallocates the odd buffers to
//make room for expansions. The expansion will probably
//success since the deallocation left room for that.

bool test_allocation_deallocation_expand()
{
   dlmalloc_malloc_check();
   std::vector<void*> buffers;

   //Allocate buffers with extra memory
   for(int i = 0; i != NumIt; ++i){
      void *ptr = dlmalloc_malloc(i);
      if(!ptr)
         break;
      buffers.push_back(ptr);
   }

   //Now deallocate the half of the blocks
   //so expand maybe can merge new free blocks
   for(int i = 0, max = (int)buffers.size()
      ;i < max
      ;++i){
      if(i%2){
         dlmalloc_free(buffers[i]);
         buffers[i] = 0;
      }
   }

   //Now try to expand to the double of the size
   for(int i = 0, max = (int)buffers.size()
      ;i < max
      ;++i){
      //
      if(buffers[i]){
         std::size_t received_size = 0;
         std::size_t min_size = i+1;
         std::size_t preferred_size = i*2;
         preferred_size = min_size > preferred_size ? min_size : preferred_size;

         while(dlmalloc_allocation_command
            ( BOOST_CONTAINER_EXPAND_FWD, 1, min_size
            , preferred_size, &received_size, (char*)buffers[i]).first){
            //Check received size is bigger than minimum
            if(received_size < min_size){
               return false;
            }
            //Now, try to expand further
            min_size       = received_size+1;
            preferred_size = min_size*2;
         }
      }
   }

   //Now erase null values from the vector
   buffers.erase(std::remove(buffers.begin(), buffers.end(), (void*)0)
                ,buffers.end());

   //Deallocate it in non sequential order
   for(int j = 0, max = (int)buffers.size()
      ;j < max
      ;++j){
      int pos = (j%4)*((int)buffers.size())/4;
      dlmalloc_free(buffers[pos]);
      buffers.erase(buffers.begin()+pos);
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();//a.all_memory_deallocated() && a.check_sanity();
}

//This test allocates until there is no more memory
//and after that deallocates all except the last.
//If the allocation algorithm is a bottom-up algorithm
//the last buffer will be in the end of the segment.
//Then the test will start expanding backwards, until
//the buffer fills all the memory

bool test_allocation_with_reuse()
{
   dlmalloc_malloc_check();
   //We will repeat this test for different sized elements
   for(int sizeof_object = 1; sizeof_object < 20; ++sizeof_object){
      std::vector<void*> buffers;

      //Allocate buffers with extra memory
      for(int i = 0; i != NumIt; ++i){
         void *ptr = dlmalloc_malloc(i*sizeof_object);
         if(!ptr)
            break;
         buffers.push_back(ptr);
      }

      //Now deallocate all except the latest
      //Now try to expand to the double of the size
      for(int i = 0, max = (int)buffers.size() - 1
         ;i < max
         ;++i){
         dlmalloc_free(buffers[i]);
      }

      //Save the unique buffer and clear vector
      void *ptr = buffers.back();
      buffers.clear();

      //Now allocate with reuse
      std::size_t received_size = 0;
      for(int i = 0; i != NumIt; ++i){
         std::size_t min_size = (received_size/sizeof_object + 1)*sizeof_object;
         std::size_t prf_size = (received_size/sizeof_object + (i+1)*2)*sizeof_object;
         dlmalloc_command_ret_t ret = dlmalloc_allocation_command
            ( BOOST_CONTAINER_EXPAND_BWD, sizeof_object, min_size
            , prf_size, &received_size, (char*)ptr);
         //If we have memory, this must be a buffer reuse
         if(!ret.first)
            break;
         //If we have memory, this must be a buffer reuse
         if(!ret.second)
            return false;
         if(received_size < min_size)
            return false;
         ptr = ret.first;
      }
      //There should be only a single block so deallocate it
      dlmalloc_free(ptr);
      dlmalloc_malloc_check();
      if(!dlmalloc_all_deallocated())
         return false;
   }
   return true;
}


//This test allocates memory with different alignments
//and checks returned memory is aligned.

bool test_aligned_allocation()
{
   dlmalloc_malloc_check();
   //Allocate aligned buffers in a loop
   //and then deallocate it
   for(unsigned int i = 1; i != (1 << (sizeof(int)/2)); i <<= 1){
      for(unsigned int j = 1; j != 512; j <<= 1){
         void *ptr = dlmalloc_memalign(i-1, j);
         if(!ptr){
            return false;
         }

         if(((std::size_t)ptr & (j - 1)) != 0)
            return false;
         dlmalloc_free(ptr);
         //if(!a.all_memory_deallocated() || !a.check_sanity()){
         //   return false;
         //}
      }
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();//a.all_memory_deallocated() && a.check_sanity();
}

//This test allocates memory with different alignments
//and checks returned memory is aligned.

bool test_continuous_aligned_allocation()
{
   dlmalloc_malloc_check();
   std::vector<void*> buffers;
   //Allocate aligned buffers in a loop
   //and then deallocate it
   bool continue_loop = true;
   unsigned int MaxAlign = 4096;
   unsigned int MaxSize  = 4096;
   for(unsigned i = 1; i < MaxSize; i <<= 1){
      for(unsigned int j = 1; j < MaxAlign; j <<= 1){
         for(int k = 0; k != NumIt; ++k){
            void *ptr = dlmalloc_memalign(i-1, j);
            buffers.push_back(ptr);
            if(!ptr){
               continue_loop = false;
               break;
            }

            if(((std::size_t)ptr & (j - 1)) != 0)
               return false;
         }
         //Deallocate all
         for(int k = (int)buffers.size(); k--;){
            dlmalloc_free(buffers[k]);
         }
         buffers.clear();
         //if(!a.all_memory_deallocated() && a.check_sanity())
         //   return false;
         if(!continue_loop)
            break;
      }
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();//a.all_memory_deallocated() && a.check_sanity();
}

//This test allocates multiple values until there is no more memory
//and after that deallocates all in the inverse order
bool test_many_equal_allocation()
{
   dlmalloc_malloc_check();
   for( deallocation_type t = DirectDeallocation
      ; t != EndDeallocationType
      ; t = (deallocation_type)((int)t + 1)){
      //std::size_t free_memory = a.get_free_memory();

      std::vector<void*> buffers2;

      //Allocate buffers with extra memory
      for(int i = 0; i != NumIt; ++i){
         void *ptr = dlmalloc_malloc(i);
         if(!ptr)
            break;
         //if(!a.check_sanity())
            //return false;
         buffers2.push_back(ptr);
      }

      //Now deallocate the half of the blocks
      //so expand maybe can merge new free blocks
      for(int i = 0, max = (int)buffers2.size()
         ;i < max
         ;++i){
         if(i%2){
            dlmalloc_free(buffers2[i]);
            buffers2[i] = 0;
         }
      }

      //if(!a.check_sanity())
         //return false;

      std::vector<void*> buffers;
      for(int i = 0; i != NumIt/10; ++i){
         dlmalloc_memchain chain;
         BOOST_CONTAINER_MEMCHAIN_INIT(&chain);
         dlmalloc_multialloc_nodes((i+1)*2, i+1, BOOST_CONTAINER_DL_MULTIALLOC_DEFAULT_CONTIGUOUS, &chain);
         dlmalloc_memchain_it it = BOOST_CONTAINER_MEMCHAIN_BEGIN_IT(&chain);
         if(BOOST_CONTAINER_MEMCHAIN_IS_END_IT(chain, it))
            break;

         std::size_t n = 0;
         for(; !BOOST_CONTAINER_MEMCHAIN_IS_END_IT(chain, it); ++n){
            buffers.push_back(BOOST_CONTAINER_MEMIT_ADDR(it));
            BOOST_CONTAINER_MEMIT_NEXT(it);
         }
         if(n != std::size_t((i+1)*2))
            return false;
      }

      //if(!a.check_sanity())
         //return false;

      switch(t){
         case DirectDeallocation:
         {
            for(int j = 0, max = (int)buffers.size()
               ;j < max
               ;++j){
               dlmalloc_free(buffers[j]);
            }
         }
         break;
         case InverseDeallocation:
         {
            for(int j = (int)buffers.size()
               ;j--
               ;){
               dlmalloc_free(buffers[j]);
            }
         }
         break;
         case MixedDeallocation:
         {
            for(int j = 0, max = (int)buffers.size()
               ;j < max
               ;++j){
               int pos = (j%4)*((int)buffers.size())/4;
               dlmalloc_free(buffers[pos]);
               buffers.erase(buffers.begin()+pos);
            }
         }
         break;
         default:
         break;
      }

      //Deallocate the rest of the blocks

      //Deallocate it in non sequential order
      for(int j = 0, max = (int)buffers2.size()
         ;j < max
         ;++j){
         int pos = (j%4)*((int)buffers2.size())/4;
         dlmalloc_free(buffers2[pos]);
         buffers2.erase(buffers2.begin()+pos);
      }

      //bool ok = free_memory == a.get_free_memory() &&
               //a.all_memory_deallocated() && a.check_sanity();
      //if(!ok)  return ok;
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();
}

//This test allocates multiple values until there is no more memory
//and after that deallocates all in the inverse order

bool test_many_different_allocation()
{
   dlmalloc_malloc_check();
   const std::size_t ArraySize = 11;
   std::size_t requested_sizes[ArraySize];
   for(std::size_t i = 0; i < ArraySize; ++i){
      requested_sizes[i] = 4*i;
   }

   for( deallocation_type t = DirectDeallocation
      ; t != EndDeallocationType
      ; t = (deallocation_type)((int)t + 1)){
      //std::size_t free_memory = a.get_free_memory();

      std::vector<void*> buffers2;

      //Allocate buffers with extra memory
      for(int i = 0; i != NumIt; ++i){
         void *ptr = dlmalloc_malloc(i);
         if(!ptr)
            break;
         buffers2.push_back(ptr);
      }

      //Now deallocate the half of the blocks
      //so expand maybe can merge new free blocks
      for(int i = 0, max = (int)buffers2.size()
         ;i < max
         ;++i){
         if(i%2){
            dlmalloc_free(buffers2[i]);
            buffers2[i] = 0;
         }
      }

      std::vector<void*> buffers;
      for(int i = 0; i != NumIt; ++i){
         dlmalloc_memchain chain;
         BOOST_CONTAINER_MEMCHAIN_INIT(&chain);
         dlmalloc_multialloc_arrays(ArraySize, requested_sizes, 1, BOOST_CONTAINER_DL_MULTIALLOC_DEFAULT_CONTIGUOUS, &chain);
         dlmalloc_memchain_it it = BOOST_CONTAINER_MEMCHAIN_BEGIN_IT(&chain);
         if(BOOST_CONTAINER_MEMCHAIN_IS_END_IT(chain, it))
            break;
         std::size_t n = 0;
         for(; !BOOST_CONTAINER_MEMCHAIN_IS_END_IT(chain, it); ++n){
            buffers.push_back(BOOST_CONTAINER_MEMIT_ADDR(it));
            BOOST_CONTAINER_MEMIT_NEXT(it);
         }
         if(n != ArraySize)
            return false;
      }

      switch(t){
         case DirectDeallocation:
         {
            for(int j = 0, max = (int)buffers.size()
               ;j < max
               ;++j){
               dlmalloc_free(buffers[j]);
            }
         }
         break;
         case InverseDeallocation:
         {
            for(int j = (int)buffers.size()
               ;j--
               ;){
               dlmalloc_free(buffers[j]);
            }
         }
         break;
         case MixedDeallocation:
         {
            for(int j = 0, max = (int)buffers.size()
               ;j < max
               ;++j){
               int pos = (j%4)*((int)buffers.size())/4;
               dlmalloc_free(buffers[pos]);
               buffers.erase(buffers.begin()+pos);
            }
         }
         break;
         default:
         break;
      }

      //Deallocate the rest of the blocks

      //Deallocate it in non sequential order
      for(int j = 0, max = (int)buffers2.size()
         ;j < max
         ;++j){
         int pos = (j%4)*((int)buffers2.size())/4;
         dlmalloc_free(buffers2[pos]);
         buffers2.erase(buffers2.begin()+pos);
      }

      //bool ok = free_memory == a.get_free_memory() &&
               //a.all_memory_deallocated() && a.check_sanity();
      //if(!ok)  return ok;
   }
   dlmalloc_malloc_check();
   return 0 != dlmalloc_all_deallocated();
}

bool test_many_deallocation()
{
   const std::size_t ArraySize = 11;
   std::vector<dlmalloc_memchain> buffers;
   std::size_t requested_sizes[ArraySize];
   for(std::size_t i = 0; i < ArraySize; ++i){
      requested_sizes[i] = 4*i;
   }

   for(int i = 0; i != NumIt; ++i){
      dlmalloc_memchain chain;
      BOOST_CONTAINER_MEMCHAIN_INIT(&chain);
      dlmalloc_multialloc_arrays(ArraySize, requested_sizes, 1, BOOST_CONTAINER_DL_MULTIALLOC_DEFAULT_CONTIGUOUS, &chain);
      dlmalloc_memchain_it it = BOOST_CONTAINER_MEMCHAIN_BEGIN_IT(&chain);
      if(BOOST_CONTAINER_MEMCHAIN_IS_END_IT(chain, it))
         return false;
      buffers.push_back(chain);
   }
   for(int i = 0; i != NumIt; ++i){
      dlmalloc_multidealloc(&buffers[i]);
   }
   buffers.clear();

   dlmalloc_malloc_check();
   if(!dlmalloc_all_deallocated())
      return false;

   for(int i = 0; i != NumIt; ++i){
      dlmalloc_memchain chain;
      BOOST_CONTAINER_MEMCHAIN_INIT(&chain);
      dlmalloc_multialloc_nodes(ArraySize, i*4+1, BOOST_CONTAINER_DL_MULTIALLOC_DEFAULT_CONTIGUOUS, &chain);
      dlmalloc_memchain_it it = BOOST_CONTAINER_MEMCHAIN_BEGIN_IT(&chain);
      if(BOOST_CONTAINER_MEMCHAIN_IS_END_IT(chain, it))
         return false;
      buffers.push_back(chain);
   }
   for(int i = 0; i != NumIt; ++i){
      dlmalloc_multidealloc(&buffers[i]);
   }
   buffers.clear();

   dlmalloc_malloc_check();
   if(!dlmalloc_all_deallocated())
      return false;

   return true;
}

//This function calls all tests

bool test_all_allocation()
{
   std::cout << "Starting test_allocation"
             << std::endl;

   if(!test_allocation()){
      std::cout << "test_allocation_direct_deallocation failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_many_equal_allocation"
             << std::endl;

   if(!test_many_equal_allocation()){
      std::cout << "test_many_equal_allocation failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_many_different_allocation"
             << std::endl;

   if(!test_many_different_allocation()){
      std::cout << "test_many_different_allocation failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_shrink"
             << std::endl;

   if(!test_allocation_shrink()){
      std::cout << "test_allocation_shrink failed"
                << std::endl;
      return false;
   }

   if(!test_allocation_shrink_and_expand()){
      std::cout << "test_allocation_shrink_and_expand failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_expand"
             << std::endl;

   if(!test_allocation_expand()){
      std::cout << "test_allocation_expand failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_deallocation_expand"
             << std::endl;

   if(!test_allocation_deallocation_expand()){
      std::cout << "test_allocation_deallocation_expand failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_with_reuse"
             << std::endl;

   if(!test_allocation_with_reuse()){
      std::cout << "test_allocation_with_reuse failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_aligned_allocation"
             << std::endl;

   if(!test_aligned_allocation()){
      std::cout << "test_aligned_allocation failed"
                << std::endl;
      return false;
   }

   std::cout << "Starting test_continuous_aligned_allocation"
             << std::endl;

   if(!test_continuous_aligned_allocation()){
      std::cout << "test_continuous_aligned_allocation failed"
                << std::endl;
      return false;
   }

   if(!test_many_deallocation()){
      std::cout << "test_many_deallocation failed"
                << std::endl;
      return false;
   }

   return 0 != dlmalloc_all_deallocated();
}

}}}   //namespace boost { namespace container { namespace test {


int main()
{
   if(!boost::container::test::test_all_allocation())
      return 1;
   return 0;
}
