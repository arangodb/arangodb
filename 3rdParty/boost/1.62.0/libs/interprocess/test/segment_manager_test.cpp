#include <boost/interprocess/indexes/flat_map_index.hpp>
#include <boost/interprocess/indexes/map_index.hpp>
#include <boost/interprocess/indexes/null_index.hpp>
#include <boost/interprocess/indexes/unordered_map_index.hpp>
#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/indexes/iunordered_set_index.hpp>

#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/segment_manager.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/interprocess/exceptions.hpp>
#include "get_process_id_name.hpp"
#include <cstddef>
#include <new>
#include <cstring>

using namespace boost::interprocess;

template <class SegmentManager>
struct atomic_func_test
{
   SegmentManager &rsm;
   int *object;

   atomic_func_test(SegmentManager &sm)
      : rsm(sm), object()
   {}

   void operator()()
   {
      object = rsm.template find<int>("atomic_func_find_object").first;
   }
   private:
   atomic_func_test operator=(const atomic_func_test&);
   atomic_func_test(const atomic_func_test&);
};

template <class SegmentManager>
bool test_segment_manager()
{
   typedef typename SegmentManager::size_type size_type;
   const unsigned int ShmSizeSize = 1024*64u;
   std::string shmname(test::get_process_id_name());

   shared_memory_object::remove(shmname.c_str());
   shared_memory_object sh_mem( create_only, shmname.c_str(), read_write );
   sh_mem.truncate( ShmSizeSize );
   mapped_region mapping( sh_mem, read_write );

   SegmentManager* seg_mgr = new( mapping.get_address() ) SegmentManager( ShmSizeSize );
   std::size_t free_mem_before = seg_mgr->get_free_memory();
   std::size_t size_before = seg_mgr->get_size();

   if(size_before != ShmSizeSize)
      return false;
   if(!seg_mgr->all_memory_deallocated())
      return false;
   if(seg_mgr->get_min_size() >= ShmSizeSize)
      return false;

   {//test get_free_memory() / allocate()/deallocate()
      const size_type Size = ShmSizeSize/2;
      void *mem = seg_mgr->allocate(Size+1);
      const size_type free_mem = seg_mgr->get_free_memory();
      if(free_mem >= Size)
         return false;
      if(seg_mgr->all_memory_deallocated())
         return false;
      const size_type Size2 = free_mem/2;
      void *mem2 = seg_mgr->allocate(size_type(Size2+1), std::nothrow);
      if(seg_mgr->get_free_memory() >= Size2)
         return false;
      if(seg_mgr->size(mem) < (Size+1))
         return false;
      if(seg_mgr->size(mem2) < (Size2+1))
         return false;
      seg_mgr->deallocate(mem);
      seg_mgr->deallocate(mem2);
      if(!seg_mgr->all_memory_deallocated())
         return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      try{  seg_mgr->allocate(ShmSizeSize*2);  }catch(interprocess_exception&){}
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(seg_mgr->allocate(ShmSizeSize*2, std::nothrow))
         return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
   }
   {//test allocate_aligned
      const std::size_t Alignment = 128u;
      void *mem = seg_mgr->allocate_aligned(ShmSizeSize/4, Alignment);
      if(seg_mgr->all_memory_deallocated())
         return false;
      std::size_t offset = static_cast<std::size_t>
         (static_cast<const char *>(mem) -  static_cast<const char *>(mapping.get_address()));
      if(offset & (Alignment-1))
         return false;
      void *mem2 = seg_mgr->allocate_aligned(ShmSizeSize/4, Alignment, std::nothrow);
      std::size_t offset2 = static_cast<std::size_t>
         (static_cast<const char *>(mem2) -  static_cast<const char *>(mapping.get_address()));
      if(offset2 & (Alignment-1))
         return false;
      seg_mgr->deallocate(mem);
      seg_mgr->deallocate(mem2);
      if(!seg_mgr->all_memory_deallocated())
         return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      try{  seg_mgr->allocate_aligned(ShmSizeSize*2, Alignment);  }catch(interprocess_exception&){}
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(seg_mgr->allocate_aligned(ShmSizeSize*2, Alignment, std::nothrow))
         return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
   }
   {//test shrink_to_fit

      seg_mgr->shrink_to_fit();
      if(!seg_mgr->all_memory_deallocated())
         return false;
      std::size_t empty_shrunk_size     = seg_mgr->get_size();
      std::size_t empty_shrunk_free_mem = seg_mgr->get_free_memory();
      if(empty_shrunk_size >= size_before)
         return false;
      if(empty_shrunk_free_mem >= size_before)
         return false;
      seg_mgr->grow(size_type(size_before - empty_shrunk_size));
      if(seg_mgr->get_size() != size_before)
         return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }
   {//test zero_free_memory
      const size_type Size(ShmSizeSize/2+1), Size2(ShmSizeSize/8);
      void *mem  = seg_mgr->allocate(Size);
      void *mem2 = seg_mgr->allocate(Size2);
      //Mark memory to non-zero
      std::memset(mem,  0xFF, Size);
      std::memset(mem2, 0xFF, Size2);
      //Deallocate and check still non-zero
      seg_mgr->deallocate(mem);
      seg_mgr->deallocate(mem2);
      {  //Use byte per byte comparison as "static unsigned char zerobuf[Size]"
         //seems to be problematic in some compilers
         unsigned char *const mem_uch_ptr  = static_cast<unsigned char *>(mem);
         unsigned char *const mem2_uch_ptr = static_cast<unsigned char *>(mem2);
         size_type zeroes = 0;
         for(size_type i = 0; i != Size; ++i){
            if(!mem_uch_ptr[i])
               ++zeroes;
         }
         if(zeroes == Size)
            return false;

         zeroes = 0;
         for(size_type i = 0; i != Size2; ++i){
            if(!mem2_uch_ptr[i])
               ++zeroes;
         }
         if(zeroes == Size2)
            return false;
      }
      //zero_free_memory and check it's zeroed
      seg_mgr->zero_free_memory();
      //TODO: some parts are not zeroed because they are used
      //as internal metadata, find a way to test this
      //if(std::memcmp(mem,  zerobuf, Size))
         //return false;
      //if(std::memcmp(mem2, zerobuf, Size2))
         //return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }

   {//test anonymous object
      int *int_object  = seg_mgr->template construct<int>(anonymous_instance)();
      if(1 != seg_mgr->get_instance_length(int_object))
         return false;
      if(anonymous_type != seg_mgr->get_instance_type(int_object))
         return false;
      if(seg_mgr->get_instance_name(int_object))
         return false;
      seg_mgr->destroy_ptr(int_object);
      int const int_array_values[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
      int *int_array  = seg_mgr->template construct_it<int>(anonymous_instance, std::nothrow)[10](&int_array_values[0]);
      if(10 != seg_mgr->get_instance_length(int_object))
         return false;
      if(anonymous_type != seg_mgr->get_instance_type(int_array))
         return false;
      if(seg_mgr->get_instance_name(int_array))
         return false;
      seg_mgr->destroy_ptr(int_array);
      try{  seg_mgr->template construct<int>(anonymous_instance)[ShmSizeSize]();  }catch(interprocess_exception&){}
      if(seg_mgr->template construct<int>(anonymous_instance, std::nothrow)[ShmSizeSize]())
      try{  seg_mgr->template construct_it<int>(anonymous_instance)[ShmSizeSize](&int_array_values[0]);  }catch(interprocess_exception&){}
      if(seg_mgr->template construct_it<int>(anonymous_instance, std::nothrow)[ShmSizeSize](&int_array_values[0]))
         return false;
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }

   {//test named object
      const char *const object1_name = "object1";
      const char *const object2_name = "object2";
      int const int_array_values[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

      for(std::size_t i = 0; i != 1/*4*/; ++i){
         if(seg_mgr->template find<unsigned int>(object1_name).first)
            return false;
         //Single element construction
         unsigned int *uint_object = 0;
         switch(i){
            case 0:
               uint_object = seg_mgr->template construct<unsigned int>(object1_name)();
            break;
            case 1:
               uint_object = seg_mgr->template construct<unsigned int>(object1_name, std::nothrow)();
            break;
            case 2:
               uint_object = seg_mgr->template find_or_construct<unsigned int>(object1_name)();
            break;
            case 3:
               uint_object = seg_mgr->template find_or_construct<unsigned int>(object1_name, std::nothrow)();
            break;
         }
         std::pair<unsigned int*, std::size_t> find_ret = seg_mgr->template find<unsigned int>(object1_name);
         if(uint_object != find_ret.first)
            return false;
         if(1 != find_ret.second)
            return false;
         if(1 != seg_mgr->get_instance_length(uint_object))
            return false;
         if(named_type != seg_mgr->get_instance_type(uint_object))
            return false;
         if(std::strcmp(object1_name, seg_mgr->get_instance_name(uint_object)))
            return false;
         //Array construction
         if(seg_mgr->template find<int>(object2_name).first)
            return false;
         int *int_array = 0;
         switch(i){
            case 0:
               int_array = seg_mgr->template construct_it<int>(object2_name)[10](&int_array_values[0]);
            break;
            case 1:
               int_array = seg_mgr->template construct_it<int>(object2_name, std::nothrow)[10](&int_array_values[0]);
            break;
            case 2:
               int_array = seg_mgr->template find_or_construct_it<int>(object2_name)[10](&int_array_values[0]);
            break;
            case 3:
               int_array = seg_mgr->template find_or_construct_it<int>(object2_name, std::nothrow)[10](&int_array_values[0]);
            break;
         }
         std::pair<int*, std::size_t> find_ret2 = seg_mgr->template find<int>(object2_name);
         if(int_array != find_ret2.first)
            return false;
         if(10 != find_ret2.second)
            return false;
         if(10 != seg_mgr->get_instance_length(int_array))
            return false;
         if(named_type != seg_mgr->get_instance_type(int_array))
            return false;
         if(std::strcmp(object2_name, seg_mgr->get_instance_name(int_array)))
            return false;
         if(seg_mgr->get_num_named_objects() != 2)
            return false;
         typename SegmentManager::const_named_iterator nb(seg_mgr->named_begin());
         typename SegmentManager::const_named_iterator ne(seg_mgr->named_end());
         for(std::size_t i = 0, imax = seg_mgr->get_num_named_objects(); i != imax; ++i){ ++nb; }
         if(nb != ne)
            return false;
         seg_mgr->destroy_ptr(uint_object);
         seg_mgr->template destroy<int>(object2_name);
      }
      try{  seg_mgr->template construct<unsigned int>(object1_name)[ShmSizeSize]();  }catch(interprocess_exception&){}
      if(seg_mgr->template construct<int>(object2_name, std::nothrow)[ShmSizeSize]())
      try{  seg_mgr->template construct_it<unsigned int>(object1_name)[ShmSizeSize](&int_array_values[0]);  }catch(interprocess_exception&){}
      if(seg_mgr->template construct_it<int>(object2_name, std::nothrow)[ShmSizeSize](&int_array_values[0]))
         return false;
      seg_mgr->shrink_to_fit_indexes();
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(!seg_mgr->all_memory_deallocated())
         return false;
      seg_mgr->reserve_named_objects(1);
      //In indexes with no capacity() memory won't be allocated so don't check anything was allocated.
      //if(seg_mgr->all_memory_deallocated())  return false;
      seg_mgr->shrink_to_fit_indexes();
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }

   {//test unique object
      int const int_array_values[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

      for(std::size_t i = 0; i != 4; ++i){
         if(seg_mgr->template find<unsigned int>(unique_instance).first)
            return false;
         //Single element construction
         unsigned int *uint_object = 0;
         switch(i){
            case 0:
               uint_object = seg_mgr->template construct<unsigned int>(unique_instance)();
            break;
            case 1:
               uint_object = seg_mgr->template construct<unsigned int>(unique_instance, std::nothrow)();
            break;
            case 2:
               uint_object = seg_mgr->template find_or_construct<unsigned int>(unique_instance)();
            break;
            case 3:
               uint_object = seg_mgr->template find_or_construct<unsigned int>(unique_instance, std::nothrow)();
            break;
         }
         std::pair<unsigned int*, std::size_t> find_ret = seg_mgr->template find<unsigned int>(unique_instance);
         if(uint_object != find_ret.first)
            return false;
         if(1 != find_ret.second)
            return false;
         if(1 != seg_mgr->get_instance_length(uint_object))
            return false;
         if(unique_type != seg_mgr->get_instance_type(uint_object))
            return false;
         if(std::strcmp(typeid(unsigned int).name(), seg_mgr->get_instance_name(uint_object)))
            return false;
         //Array construction
         if(seg_mgr->template find<int>(unique_instance).first)
            return false;
         int *int_array = 0;
         switch(i){
            case 0:
               int_array = seg_mgr->template construct_it<int>(unique_instance)[10](&int_array_values[0]);
            break;
            case 1:
               int_array = seg_mgr->template construct_it<int>(unique_instance, std::nothrow)[10](&int_array_values[0]);
            break;
            case 2:
               int_array = seg_mgr->template find_or_construct_it<int>(unique_instance)[10](&int_array_values[0]);
            break;
            case 3:
               int_array = seg_mgr->template find_or_construct_it<int>(unique_instance, std::nothrow)[10](&int_array_values[0]);
            break;
         }
         std::pair<int*, std::size_t> find_ret2 = seg_mgr->template find<int>(unique_instance);
         if(int_array != find_ret2.first)
            return false;
         if(10 != find_ret2.second)
            return false;
         if(10 != seg_mgr->get_instance_length(int_array))
            return false;
         if(unique_type != seg_mgr->get_instance_type(int_array))
            return false;
         if(std::strcmp(typeid(int).name(), seg_mgr->get_instance_name(int_array)))
            return false;
         if(seg_mgr->get_num_unique_objects() != 2)
            return false;
         typename SegmentManager::const_unique_iterator nb(seg_mgr->unique_begin());
         typename SegmentManager::const_unique_iterator ne(seg_mgr->unique_end());
         for(std::size_t i = 0, imax = seg_mgr->get_num_unique_objects(); i != imax; ++i){ ++nb; }
         if(nb != ne)
            return false;
         seg_mgr->destroy_ptr(uint_object);
         seg_mgr->template destroy<int>(unique_instance);
      }
      try{  seg_mgr->template construct<unsigned int>(unique_instance)[ShmSizeSize]();  }catch(interprocess_exception&){}
      if(seg_mgr->template construct<int>(unique_instance, std::nothrow)[ShmSizeSize]())
      try{  seg_mgr->template construct_it<unsigned int>(unique_instance)[ShmSizeSize](&int_array_values[0]);  }catch(interprocess_exception&){}
      if(seg_mgr->template construct_it<int>(unique_instance, std::nothrow)[ShmSizeSize](&int_array_values[0]))
         return false;
      seg_mgr->shrink_to_fit_indexes();
      if(seg_mgr->get_free_memory() != free_mem_before)
         return false;
      if(!seg_mgr->all_memory_deallocated())
         return false;
      seg_mgr->reserve_unique_objects(1);
      //In indexes with no capacity() memory won't be allocated so don't check anything was allocated.
      //if(seg_mgr->all_memory_deallocated())  return false;
      seg_mgr->shrink_to_fit_indexes();
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }
   {//test allocator/deleter
      if(!seg_mgr->all_memory_deallocated())
         return false;
      typedef typename SegmentManager::template allocator<float>::type allocator_t;

      allocator_t alloc(seg_mgr->template get_allocator<float>());

      if(!seg_mgr->all_memory_deallocated())
         return false;
      offset_ptr<float> f = alloc.allocate(50);
      if(seg_mgr->all_memory_deallocated())
         return false;
      alloc.deallocate(f, 50);
      if(!seg_mgr->all_memory_deallocated())
         return false;
      typedef typename SegmentManager::template deleter<float>::type deleter_t;
      deleter_t delet(seg_mgr->template get_deleter<float>());
      delet(seg_mgr->template construct<float>(anonymous_instance)());
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }
   {//test allocator/deleter
      if(!seg_mgr->all_memory_deallocated())
         return false;
      int *int_object  = seg_mgr->template construct<int>("atomic_func_find_object")();
      atomic_func_test<SegmentManager> func(*seg_mgr);
      seg_mgr->atomic_func(func);
      if(int_object != func.object)
         return 1;
      seg_mgr->destroy_ptr(int_object);
      seg_mgr->shrink_to_fit_indexes();
      if(!seg_mgr->all_memory_deallocated())
         return false;
   }
   return true;
}

template<class MemoryAlgorithm>
bool test_each_algo()
{
   {
      typedef segment_manager< char, MemoryAlgorithm, flat_map_index > segment_manager_t;
      if(!test_segment_manager<segment_manager_t>())
         return false;
   }
   {
      typedef segment_manager< char, MemoryAlgorithm, map_index > segment_manager_t;
      if(!test_segment_manager<segment_manager_t>())
         return false;
   }
   /*
   {
      typedef segment_manager< char, MemoryAlgorithm, null_index > segment_manager_t;
      if(!test_segment_manager<segment_manager_t>())
         return false;
   }*/
   /*
   {
      typedef segment_manager< char, MemoryAlgorithm, unordered_map_index > segment_manager_t;
      if(!test_segment_manager<segment_manager_t>())
         return false;
   }*/
   {
      typedef segment_manager< char, MemoryAlgorithm, iset_index > segment_manager_t;
      if(!test_segment_manager<segment_manager_t>())
         return false;
   }
   {
      typedef segment_manager< char, MemoryAlgorithm, iunordered_set_index > segment_manager_t;
      if(!test_segment_manager<segment_manager_t>())
         return false;
   }
   return true;
}

int main()
{
   if(!test_each_algo< simple_seq_fit< null_mutex_family > >())
      return 1;
   if(!test_each_algo< rbtree_best_fit< null_mutex_family > >())
      return 1;

   return 0;
}
