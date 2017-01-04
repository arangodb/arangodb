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
#pragma warning (disable : 4267)
#pragma warning (disable : 4244)
#endif

#define BOOST_CONTAINER_VECTOR_ALLOC_STATS

#include <boost/container/allocator.hpp>
#include <vector>
#include <boost/container/vector.hpp>

#include <memory>    //std::allocator
#include <iostream>  //std::cout, std::endl
#include <cstring>   //std::strcmp
#include <boost/timer/timer.hpp>
using boost::timer::cpu_timer;
using boost::timer::cpu_times;
using boost::timer::nanosecond_type;

namespace bc = boost::container;

typedef std::allocator<int>   StdAllocator;
typedef bc::allocator<int, 2, bc::expand_bwd | bc::expand_fwd> AllocatorPlusV2Mask;
typedef bc::allocator<int, 2> AllocatorPlusV2;
typedef bc::allocator<int, 1> AllocatorPlusV1;

template<class Allocator> struct get_allocator_name;

template<> struct get_allocator_name<StdAllocator>
{  static const char *get() {  return "StdAllocator";  } };

template<> struct get_allocator_name<AllocatorPlusV2Mask>
{  static const char *get() {  return "AllocatorPlusV2Mask";  }   };

template<> struct get_allocator_name<AllocatorPlusV2>
{  static const char *get() {  return "AllocatorPlusV2";  } };

template<> struct get_allocator_name<AllocatorPlusV1>
{  static const char *get() {  return "AllocatorPlusV1";  } };

#if defined(BOOST_CONTAINER_VECTOR_ALLOC_STATS)
//
// stats_traits;
//

template<template<class, class> class Vector>
struct stats_traits;

template<>
struct stats_traits<std::vector>
{
   template<class T, class Allocator>
   static void reset_alloc_stats(std::vector<T, Allocator> &)
      {}

   template<class T, class Allocator>
   static std::size_t get_num_alloc(std::vector<T, Allocator> &)
      {  return 0;   }

   template<class T, class Allocator>
   static std::size_t get_num_expand(std::vector<T, Allocator> &)
      {  return 0;   }
};

template<>
struct stats_traits<bc::vector>
{
   template<class T, class Allocator>
   static void reset_alloc_stats(bc::vector<T, Allocator> &v)
      { v.reset_alloc_stats(); }

   template<class T, class Allocator>
   static std::size_t get_num_alloc(bc::vector<T, Allocator> &v)
      {  return v.num_alloc;  }

   template<class T, class Allocator>
   static std::size_t get_num_expand(bc::vector<T, Allocator> &v)
      {  return v.num_expand_fwd;  }
};

#endif   //BOOST_CONTAINER_VECTOR_ALLOC_STATS

template<template<class, class> class Vector> struct get_container_name;

template<> struct get_container_name<std::vector>
{  static const char *get() {  return "StdVector";  } };

template<> struct get_container_name<bc::vector>
{  static const char *get() {  return "BoostContainerVector";  } };

class MyInt
{
   int int_;

   public:
   explicit MyInt(int i = 0)
      : int_(i)
   {}

   MyInt(const MyInt &other)
      :  int_(other.int_)
   {}

   MyInt & operator=(const MyInt &other)
   {
      int_ = other.int_;
      return *this;
   }

   ~MyInt()
   {
      int_ = 0;
   }
};

template<class Allocator, template <class, class> class Vector>
void vector_test_template(unsigned int num_iterations, unsigned int num_elements, bool csv_output)
{
   typedef typename Allocator::template rebind<MyInt>::other IntAllocator;
   unsigned int numalloc = 0, numexpand = 0;

   #ifdef BOOST_CONTAINER_VECTOR_ALLOC_STATS
      typedef stats_traits<Vector> stats_traits_t;
   #endif

   cpu_timer timer;
   timer.resume();

   unsigned int capacity = 0;
   for(unsigned int r = 0; r != num_iterations; ++r){
      Vector<MyInt, IntAllocator> v;
      #ifdef BOOST_CONTAINER_VECTOR_ALLOC_STATS
         stats_traits_t::reset_alloc_stats(v);
      #endif
      //v.reserve(num_elements);
      //MyInt a[3];
/*
      for(unsigned int e = 0; e != num_elements/3; ++e){
         v.insert(v.end(), &a[0], &a[0]+3);
      }*/
/*
      for(unsigned int e = 0; e != num_elements/3; ++e){
         v.insert(v.end(), 3, MyInt(e));
      }*/
/*
      for(unsigned int e = 0; e != num_elements/3; ++e){
         v.insert(v.empty() ? v.end() : --v.end(), &a[0], &a[0]+3);
      }*/
/*
      for(unsigned int e = 0; e != num_elements/3; ++e){
         v.insert(v.empty() ? v.end() : --v.end(), 3, MyInt(e));
      }*/
/*
      for(unsigned int e = 0; e != num_elements/3; ++e){
         v.insert(v.size() >= 3 ? v.end()-3 : v.begin(), &a[0], &a[0]+3);
      }*/
/*
      for(unsigned int e = 0; e != num_elements/3; ++e){
         v.insert(v.size() >= 3 ? v.end()-3 : v.begin(), 3, MyInt(e));
      }*/
/*
      for(unsigned int e = 0; e != num_elements; ++e){
         v.insert(v.end(), MyInt(e));
      }*/
/*
      for(unsigned int e = 0; e != num_elements; ++e){
         v.insert(v.empty() ? v.end() : --v.end(), MyInt(e));
      }*/

      for(unsigned int e = 0; e != num_elements; ++e){
         v.push_back(MyInt(e));
      }

      #ifdef BOOST_CONTAINER_VECTOR_ALLOC_STATS
         numalloc  += stats_traits_t::get_num_alloc(v);
         numexpand += stats_traits_t::get_num_expand(v);
      #endif
      capacity = static_cast<unsigned int>(v.capacity());
   }

   timer.stop();
   nanosecond_type nseconds = timer.elapsed().wall;

   if(csv_output){
      std::cout   << get_allocator_name<Allocator>::get()
                  << ";"
                  << num_iterations
                  << ";"
                  << num_elements
                  << ";"
                  << capacity
                  << ";"
                  << float(nseconds)/(num_iterations*num_elements)
                  << ";"
                  << (float(numalloc) + float(numexpand))/num_iterations
                  << ";"
                  << float(numalloc)/num_iterations
                  << ";"
                  << float(numexpand)/num_iterations
                  << std::endl;
   }
   else{
      std::cout   << std::endl
                  << "Allocator: " << get_allocator_name<Allocator>::get()
                  << std::endl
                  << "  push_back ns:              "
                  << float(nseconds)/(num_iterations*num_elements)
                  << std::endl
                  << "  capacity  -  alloc calls (new/expand):  "
                     << (unsigned int)capacity << "  -  "
                     << (float(numalloc) + float(numexpand))/num_iterations
                     << "(" << float(numalloc)/num_iterations << "/" << float(numexpand)/num_iterations << ")"
                  << std::endl << std::endl;
   }
   bc::dlmalloc_trim(0);
}

void print_header()
{
   std::cout   << "Allocator" << ";" << "Iterations" << ";" << "Size" << ";"
               << "Capacity" << ";" << "push_back(ns)" << ";" << "Allocator calls" << ";"
               << "New allocations" << ";" << "Fwd expansions" << std::endl;
}

int main(int argc, const char *argv[])
{
   #define SINGLE_TEST
   #ifndef SINGLE_TEST
      #ifdef NDEBUG
      unsigned int numit []  = { 1000, 10000, 100000, 1000000 };
      #else
      unsigned int numit []  = { 100, 1000, 10000, 100000 };
      #endif
      unsigned int numele [] = { 10000, 1000,   100,     10       };
   #else
      #ifdef NDEBUG
      std::size_t numit [] = { 1000 };
      #else
      std::size_t numit [] = { 100 };
      #endif
      std::size_t numele [] = { 10000 };
   #endif

   bool csv_output = argc == 2 && (strcmp(argv[1], "--csv-output") == 0);

   if(csv_output){
      print_header();
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<StdAllocator, bc::vector>(numit[i], numele[i], csv_output);
      }
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV1, bc::vector>(numit[i], numele[i], csv_output);
      }
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV2Mask, bc::vector>(numit[i], numele[i], csv_output);
      }
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV2, bc::vector>(numit[i], numele[i], csv_output);
      }
   }
   else{
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         std::cout   << "\n    -----------------------------------    \n"
                     <<   "  Iterations/Elements:         " << numit[i] << "/" << numele[i]
                     << "\n    -----------------------------------    \n";
         vector_test_template<StdAllocator, std::vector>(numit[i], numele[i], csv_output);
         vector_test_template<StdAllocator, bc::vector>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV1, bc::vector>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV2Mask, bc::vector>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV2, bc::vector>(numit[i], numele[i], csv_output);
      }
   }
   return 0;
}
