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
#pragma warning (disable : 4512)
#pragma warning (disable : 4541)
#pragma warning (disable : 4673)
#pragma warning (disable : 4671)
#pragma warning (disable : 4244)
#endif

#include <memory>    //std::allocator
#include <iostream>  //std::cout, std::endl
#include <vector>    //std::vector
#include <cstddef>   //std::size_t
#include <cassert>   //assert

#include <boost/container/allocator.hpp>
#include <boost/container/adaptive_pool.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/vector.hpp>
#include <boost/move/detail/nsec_clock.hpp>

using boost::move_detail::cpu_timer;
using boost::move_detail::cpu_times;
using boost::move_detail::nanosecond_type;

namespace bc = boost::container;

class MyInt
{
   int int_;

   public:
   MyInt(int i = 0) : int_(i){}
   MyInt(const MyInt &other)
      :  int_(other.int_)
   {}
   MyInt & operator=(const MyInt &other)
   {
      int_ = other.int_;
      return *this;
   }
};

typedef std::allocator<MyInt>   StdAllocator;
typedef bc::allocator<MyInt, 1> AllocatorPlusV1;
typedef bc::allocator<MyInt, 2> AllocatorPlusV2;
typedef bc::adaptive_pool
   < MyInt
   , bc::ADP_nodes_per_block
   , 0//bc::ADP_max_free_blocks
   , 2
   , 2>                       AdPool2PercentV2;

template<class Allocator> struct get_allocator_name;

template<> struct get_allocator_name<StdAllocator>
{  static const char *get() {  return "StdAllocator";  } };

template<> struct get_allocator_name<AllocatorPlusV1>
{  static const char *get() {  return "AllocatorPlusV1";  } };

template<> struct get_allocator_name<AllocatorPlusV2>
{  static const char *get() {  return "AllocatorPlusV2";  } };

template<> struct get_allocator_name<AdPool2PercentV2>
{  static const char *get() {  return "AdPool2PercentV2";  } };

template<class Allocator>
struct get_vector
{
   typedef bc::vector<MyInt, Allocator> type;
   static const char *vector_name()
   {
      return "vector<MyInt>";
   }
};

template<class Allocator>
struct get_stable_vector
{
   typedef bc::stable_vector
      <MyInt, Allocator> type;
   static const char *vector_name()
   {
      return "stable_vector<MyInt>";
   }
};

template<template<class> class GetContainer, class Allocator>
void stable_vector_test_template(std::size_t num_iterations, std::size_t num_elements, bool csv_output)
{
   typedef typename GetContainer<Allocator>::type vector_type;
   //std::size_t top_capacity = 0;
   nanosecond_type nseconds;
   {
      {
         vector_type l;
         cpu_timer timer;
         timer.resume();

         for(std::size_t r = 0; r != num_iterations; ++r){
            l.insert(l.end(), num_elements, MyInt((int)r));
         }

         timer.stop();
         nseconds = timer.elapsed().wall;

         if(csv_output){
            std::cout   << get_allocator_name<Allocator>::get()
                        << ";"
                        << GetContainer<Allocator>::vector_name()
                        << ";"
                        << num_iterations
                        << ";"
                        << num_elements
                        << ";"
                        << float(nseconds)/float(num_iterations*num_elements)
                        << ";";
         }
         else{
            std::cout   << "Allocator: " << get_allocator_name<Allocator>::get()
                        << '\t'
                        << GetContainer<Allocator>::vector_name()
                        << std::endl
                        << "  allocation ns:   "
                        << float(nseconds)/float(num_iterations*num_elements);
         }
//         top_capacity = l.capacity();
         //Now preprocess ranges to erase
         std::vector<typename vector_type::iterator> ranges_to_erase;
         ranges_to_erase.push_back(l.begin());
         for(std::size_t r = 0; r != num_iterations; ++r){
            typename vector_type::iterator next_pos(ranges_to_erase[r]);
            std::size_t n = num_elements;
            while(n--){ ++next_pos; }
            ranges_to_erase.push_back(next_pos);
         }

         //Measure range erasure function
         timer.stop();
         timer.start();

         for(std::size_t r = 0; r != num_iterations; ++r){
            std::size_t init_pos = (num_iterations-1)-r;
            l.erase(ranges_to_erase[init_pos], l.end());
         }
         timer.stop();
         nseconds = timer.elapsed().wall;
         assert(l.empty());
      }
   }

   if(csv_output){
      std::cout      << float(nseconds)/float(num_iterations*num_elements)
                     << std::endl;
   }
   else{
      std::cout      << '\t'
                     << "  deallocation ns: "
                     << float(nseconds)/float(num_iterations*num_elements)/*
                     << std::endl
                     << "  max capacity:    "
                     << static_cast<std::size_t>(top_capacity)
                     << std::endl
                     << "  remaining cap.   "
                     << static_cast<std::size_t>(top_capacity - num_iterations*num_elements)
                     << " (" << (float(top_capacity)/float(num_iterations*num_elements) - 1)*100 << " %)"*/
                     << std::endl << std::endl;
   }
   assert(bc::dlmalloc_all_deallocated());
   bc::dlmalloc_trim(0);
}

void print_header()
{
   std::cout   << "Allocator" << ";" << "Iterations" << ";" << "Size" << ";"
               << "Insertion time(ns)" << ";" << "Erasure time(ns)" << ";"
               << std::endl;
}

void stable_vector_operations()
{
   {
      bc::stable_vector<int> a(bc::stable_vector<int>::size_type(5), 4);
      bc::stable_vector<int> b(a);
      bc::stable_vector<int> c(a.cbegin(), a.cend());
      b.insert(b.cend(), 0);
      c.pop_back();
      a.assign(b.cbegin(), b.cend());
      a.assign(c.cbegin(), c.cend());
      a.assign(1, 2);
   }
   {
      typedef bc::stable_vector<int, std::allocator<int> > stable_vector_t;
      stable_vector_t a(bc::stable_vector<int>::size_type(5), 4);
      stable_vector_t b(a);
      stable_vector_t c(a.cbegin(), a.cend());
      b.insert(b.cend(), 0);
      c.pop_back();
      assert(static_cast<std::size_t>(a.end() - a.begin()) == a.size());
      a.assign(b.cbegin(), b.cend());
      assert(static_cast<std::size_t>(a.end() - a.begin()) == a.size());
      a.assign(c.cbegin(), c.cend());
      assert(static_cast<std::size_t>(a.end() - a.begin()) == a.size());
      a.assign(1, 2);
      assert(static_cast<std::size_t>(a.end() - a.begin()) == a.size());
      a.reserve(100);
      assert(static_cast<std::size_t>(a.end() - a.begin()) == a.size());
   }
}

int main(int argc, const char *argv[])
{
   //#define SINGLE_TEST
   #define SIMPLE_IT
   #ifdef SINGLE_TEST
      #ifdef NDEBUG
      std::size_t numit [] = { 40 };
      #else
      std::size_t numit [] = { 4 };
      #endif
      std::size_t numele [] = { 10000 };
   #elif defined(SIMPLE_IT)
      std::size_t numit [] = { 3 };
      std::size_t numele [] = { 10000 };
   #else
      #ifdef NDEBUG
      std::size_t numit [] = { 40, 400, 4000, 40000 };
      #else
      std::size_t numit [] = { 4,   40,   400,   4000 };
      #endif
      std::size_t numele [] = { 10000, 1000, 100,   10     };
   #endif

   //Warning: range erasure is buggy. Vector iterators are not stable, so it is not
   //possible to cache iterators, but indexes!!!

   bool csv_output = argc == 2 && (strcmp(argv[1], "--csv-output") == 0);

   if(csv_output){
      print_header();
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_stable_vector, StdAllocator>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_vector, StdAllocator>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_stable_vector, AllocatorPlusV1>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_vector, AllocatorPlusV1>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_stable_vector, AllocatorPlusV2>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_vector, AllocatorPlusV2>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_stable_vector, AdPool2PercentV2>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         stable_vector_test_template<get_vector, AdPool2PercentV2>(numit[i], numele[i], csv_output);
      }
   }
   else{
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         std::cout   << "\n    -----------------------------------    \n"
                     <<   "  Iterations/Elements:         " << numit[i] << "/" << numele[i]
                     << "\n    -----------------------------------    \n";
         stable_vector_test_template<get_stable_vector, StdAllocator>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_vector, StdAllocator>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_stable_vector, AllocatorPlusV1>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_vector, AllocatorPlusV1>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_stable_vector, AllocatorPlusV2>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_vector, AllocatorPlusV2>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_stable_vector, AdPool2PercentV2>(numit[i], numele[i], csv_output);
         stable_vector_test_template<get_vector, AdPool2PercentV2>(numit[i], numele[i], csv_output);
      }
   }

   return 0;
}
