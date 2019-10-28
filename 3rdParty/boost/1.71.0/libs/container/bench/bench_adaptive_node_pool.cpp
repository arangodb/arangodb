//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//Enable checks in debug mode
#ifndef NDEBUG
#define BOOST_CONTAINER_ADAPTIVE_NODE_POOL_CHECK_INVARIANTS
#endif

#ifdef _MSC_VER
#pragma warning (disable : 4512)
#pragma warning (disable : 4127)
#pragma warning (disable : 4244)
#pragma warning (disable : 4267)
#endif

#include <boost/container/adaptive_pool.hpp>
#include <boost/container/node_allocator.hpp>
#include <boost/container/allocator.hpp>
#include <boost/container/list.hpp>
#include <memory>    //std::allocator
#include <iostream>  //std::cout, std::endl
#include <vector>    //std::vector
#include <cstddef>   //std::size_t
#include <cassert>   //assert

#include <boost/timer/timer.hpp>
using boost::timer::cpu_timer;
using boost::timer::cpu_times;
using boost::timer::nanosecond_type;

namespace bc = boost::container;

typedef std::allocator<int>         StdAllocator;
typedef bc::allocator<int, 2>       AllocatorPlusV2;
typedef bc::allocator<int, 1>       AllocatorPlusV1;
typedef bc::adaptive_pool
   < int
   , bc::ADP_nodes_per_block
   , bc::ADP_max_free_blocks
   , bc::ADP_only_alignment
   , 1>                             AdPoolAlignOnlyV1;
typedef bc::adaptive_pool
   < int
   , bc::ADP_nodes_per_block
   , bc::ADP_max_free_blocks
   , bc::ADP_only_alignment
   , 2>                             AdPoolAlignOnlyV2;
typedef bc::adaptive_pool
   < int
   , bc::ADP_nodes_per_block
   , bc::ADP_max_free_blocks
   , 2
   , 1>                             AdPool2PercentV1;
typedef bc::adaptive_pool
   < int
   , bc::ADP_nodes_per_block
   , bc::ADP_max_free_blocks
   , 2
   , 2>                             AdPool2PercentV2;
typedef bc::node_allocator
   < int
   , bc::NodeAlloc_nodes_per_block
   , 1>                             SimpleSegregatedStorageV1;
typedef bc::node_allocator
   < int
   , bc::NodeAlloc_nodes_per_block
   , 2>                             SimpleSegregatedStorageV2;

//Explicit instantiation
template class bc::adaptive_pool
   < int
   , bc::ADP_nodes_per_block
   , bc::ADP_max_free_blocks
   , bc::ADP_only_alignment
   , 2>;

template class bc::node_allocator
   < int
   , bc::NodeAlloc_nodes_per_block
   , 2>;

template<class Allocator> struct get_allocator_name;

template<> struct get_allocator_name<StdAllocator>
{  static const char *get() {  return "StdAllocator";  } };

template<> struct get_allocator_name<AllocatorPlusV2>
{  static const char *get() {  return "AllocatorPlusV2";  } };

template<> struct get_allocator_name<AllocatorPlusV1>
{  static const char *get() {  return "AllocatorPlusV1";  } };

template<> struct get_allocator_name<AdPoolAlignOnlyV1>
{  static const char *get() {  return "AdPoolAlignOnlyV1";  } };

template<> struct get_allocator_name<AdPoolAlignOnlyV2>
{  static const char *get() {  return "AdPoolAlignOnlyV2";  } };

template<> struct get_allocator_name<AdPool2PercentV1>
{  static const char *get() {  return "AdPool2PercentV1";  } };

template<> struct get_allocator_name<AdPool2PercentV2>
{  static const char *get() {  return "AdPool2PercentV2";  } };

template<> struct get_allocator_name<SimpleSegregatedStorageV1>
{  static const char *get() {  return "SimpleSegregatedStorageV1";  } };

template<> struct get_allocator_name<SimpleSegregatedStorageV2>
{  static const char *get() {  return "SimpleSegregatedStorageV2";  } };

class MyInt
{
   std::size_t int_;

   public:
   explicit MyInt(std::size_t i = 0) : int_(i){}
   MyInt(const MyInt &other)
      :  int_(other.int_)
   {}
   MyInt & operator=(const MyInt &other)
   {
      int_ = other.int_;
      return *this;
   }
};

template<class Allocator>
void list_test_template(std::size_t num_iterations, std::size_t num_elements, bool csv_output)
{
   typedef typename Allocator::template rebind<MyInt>::other IntAllocator;
   nanosecond_type tinsert, terase;
   bc::dlmalloc_malloc_stats_t insert_stats, erase_stats;
   std::size_t insert_inuse, erase_inuse;
   const size_t sizeof_node = 2*sizeof(void*)+sizeof(int);

   typedef bc::list<MyInt, IntAllocator>  list_t;
   typedef typename list_t::iterator      iterator_t;
   {
      cpu_timer timer;
      timer.resume();
      list_t l;
      for(std::size_t r = 0; r != num_iterations; ++r){
         l.insert(l.end(), num_elements, MyInt(r));
      }
      timer.stop();
      tinsert = timer.elapsed().wall;

      insert_inuse = bc::dlmalloc_in_use_memory();
      insert_stats = bc::dlmalloc_malloc_stats();
/*
      iterator_t it(l.begin());
      iterator_t last(--l.end());
      for(std::size_t n_elem = 0, n_max = l.size()/2-1; n_elem != n_max; ++n_elem)
      {
         l.splice(it++, l, last--);
      }
*/
      //l.reverse();

      //Now preprocess erase ranges
      std::vector<iterator_t> ranges_to_erase;
      ranges_to_erase.push_back(l.begin());
      for(std::size_t r = 0; r != num_iterations; ++r){
         iterator_t next_pos(ranges_to_erase[r]);
         std::size_t n = num_elements;
         while(n--){ ++next_pos; }
         ranges_to_erase.push_back(next_pos);
      }

      //Measure range erasure function
      timer.start();
      for(std::size_t r = 0; r != num_iterations; ++r){
         assert((r+1) < ranges_to_erase.size());
         l.erase(ranges_to_erase[r], ranges_to_erase[r+1]);
      }
      timer.stop();
      terase = timer.elapsed().wall;
      erase_inuse = bc::dlmalloc_in_use_memory();
      erase_stats = bc::dlmalloc_malloc_stats();
   }


   if(csv_output){
      std::cout   << get_allocator_name<Allocator>::get()
                  << ";"
                  << num_iterations
                  << ";"
                  << num_elements
                  << ";"
                  << float(tinsert)/(num_iterations*num_elements)
                  << ";"
                  << (unsigned int)insert_stats.system_bytes
                  << ";"
                  << float(insert_stats.system_bytes)/(num_iterations*num_elements*sizeof_node)*100.0-100.0
                  << ";"
                  << (unsigned int)insert_inuse
                  << ";"
                  << (float(insert_inuse)/(num_iterations*num_elements*sizeof_node)*100.0)-100.0
                  << ";";
   std::cout   << float(terase)/(num_iterations*num_elements)
               << ";"
               << (unsigned int)erase_stats.system_bytes
               << ";"
               << (unsigned int)erase_inuse
               << std::endl;
   }
   else{
      std::cout << std::endl
               << "Allocator: " << get_allocator_name<Allocator>::get()
               << std::endl
               << "  allocation/deallocation(ns): " << float(tinsert)/(num_iterations*num_elements) <<  '\t' << float(terase)/(num_iterations*num_elements)
               << std::endl
               << "  Sys MB(overh.)/Inuse MB(overh.): " << (float)insert_stats.system_bytes/(1024*1024) << "(" << float(insert_stats.system_bytes)/(num_iterations*num_elements*sizeof_node)*100.0-100.0 << "%)"
               << " / "
               << (float)insert_inuse/(1024*1024) << "(" << (float(insert_inuse)/(num_iterations*num_elements*sizeof_node)*100.0)-100.0 << "%)"
               << std::endl
               << "  system MB/inuse bytes after:    " << (float)erase_stats.system_bytes/(1024*1024) << '\t' << bc::dlmalloc_in_use_memory()
               << std::endl  << std::endl;
   }

   //Release node_allocator cache
   typedef boost::container::dtl::shared_node_pool
      < (2*sizeof(void*)+sizeof(int))
      , AdPoolAlignOnlyV2::nodes_per_block> shared_node_pool_t;
   boost::container::dtl::singleton_default
      <shared_node_pool_t>::instance().purge_blocks();

   //Release adaptive_pool cache
   typedef boost::container::dtl::shared_adaptive_node_pool
      < (2*sizeof(void*)+sizeof(int))
      , AdPool2PercentV2::nodes_per_block
      , AdPool2PercentV2::max_free_blocks
      , AdPool2PercentV2::overhead_percent> shared_adaptive_pool_plus_t;
   boost::container::dtl::singleton_default
      <shared_adaptive_pool_plus_t>::instance().deallocate_free_blocks();

   //Release adaptive_pool cache
   typedef boost::container::dtl::shared_adaptive_node_pool
      < (2*sizeof(void*)+sizeof(int))
      , AdPool2PercentV2::nodes_per_block
      , AdPool2PercentV2::max_free_blocks
      , 0u> shared_adaptive_pool_plus_align_only_t;
   boost::container::dtl::singleton_default
      <shared_adaptive_pool_plus_align_only_t>::instance().deallocate_free_blocks();
   //Release dlmalloc memory
   bc::dlmalloc_trim(0);
}

void print_header()
{
   std::cout   << "Allocator" << ";" << "Iterations" << ";" << "Size" << ";"
               << "Insertion time(ns)" << ";"
               << "System bytes" << ";"
               << "System overhead(%)" << ";"
               << "In use bytes" << ";"
               << "In use overhead(%)" << ";"
               << "Erasure time (ns)" << ";"
               << "System bytes after" << ";"
               << "In use bytes after"
               << std::endl;
}

int main(int argc, const char *argv[])
{
   //#define SINGLE_TEST
   #define SIMPLE_IT
   #ifdef SINGLE_TEST
      #ifdef BOOST_CONTAINER_ADAPTIVE_NODE_POOL_CHECK_INVARIANTS
      std::size_t numrep[] = { 1000 };
      #elif defined(NDEBUG)
      std::size_t numrep [] = { 15000 };
      #else
      std::size_t numrep [] = { 1000 };
      #endif
      std::size_t numele [] = { 100 };
   #elif defined(SIMPLE_IT)
      std::size_t numrep [] = { 3 };
      std::size_t numele [] = { 100 };
   #else
      #ifdef NDEBUG
      std::size_t numrep [] = { 300, 3000, 30000, 300000, 600000, 1500000, 3000000 };
      #else
      std::size_t numrep [] = { 20,   200, 2000, 20000, 40000, 100000, 200000 };
      #endif
      std::size_t numele [] = { 10000, 1000, 100, 10, 5, 2, 1     };
   #endif

   bool csv_output = argc == 2 && (strcmp(argv[1], "--csv-output") == 0);

   if(csv_output){/*
      print_header();
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<AllocatorPlusV1>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<AllocatorPlusV2>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<AdPoolAlignOnlyV1>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<AdPoolAlignOnlyV2>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<AdPool2PercentV1>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<AdPool2PercentV2>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<SimpleSegregatedStorageV1>(numrep[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         list_test_template<SimpleSegregatedStorageV2>(numrep[i], numele[i], csv_output);
      }*/
   }
   else{
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         std::cout   << "\n    -----------------------------------    \n"
                     <<   "  Iterations/Elements:         " << numrep[i] << "/" << numele[i]
                     << "\n    -----------------------------------    \n";
         list_test_template<AllocatorPlusV1>(numrep[i], numele[i], csv_output);
         list_test_template<AllocatorPlusV2>(numrep[i], numele[i], csv_output);
         list_test_template<AdPoolAlignOnlyV1>(numrep[i], numele[i], csv_output);
         list_test_template<AdPoolAlignOnlyV2>(numrep[i], numele[i], csv_output);
         list_test_template<AdPool2PercentV1>(numrep[i], numele[i], csv_output);
         list_test_template<AdPool2PercentV2>(numrep[i], numele[i], csv_output);
         list_test_template<SimpleSegregatedStorageV1>(numrep[i], numele[i], csv_output);
         list_test_template<SimpleSegregatedStorageV2>(numrep[i], numele[i], csv_output);
      }
   }
   return 0;
}
