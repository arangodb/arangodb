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
#include <boost/move/detail/nsec_clock.hpp>
#include <typeinfo>
using boost::move_detail::cpu_timer;
using boost::move_detail::cpu_times;
using boost::move_detail::nanosecond_type;

namespace bc = boost::container;

#if defined(BOOST_CONTAINER_VECTOR_ALLOC_STATS)

template<class T, class Allocator>
static void reset_alloc_stats(std::vector<T, Allocator> &)
   {}

template<class T, class Allocator>
static std::size_t get_num_alloc(std::vector<T, Allocator> &)
   {  return 0;   }

template<class T, class Allocator>
static std::size_t get_num_expand(std::vector<T, Allocator> &)
   {  return 0;   }

template<class T, class Allocator>
static void reset_alloc_stats(bc::vector<T, Allocator> &v)
   { v.reset_alloc_stats(); }

template<class T, class Allocator>
static std::size_t get_num_alloc(bc::vector<T, Allocator> &v)
   {  return v.num_alloc;  }

template<class T, class Allocator>
static std::size_t get_num_expand(bc::vector<T, Allocator> &v)
   {  return v.num_expand_fwd;  }

#endif   //BOOST_CONTAINER_VECTOR_ALLOC_STATS

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

template<class Container>
void vector_test_template(std::size_t num_iterations, std::size_t num_elements)
{
   std::size_t numalloc = 0, numexpand = 0;

   cpu_timer timer;
   timer.resume();

   std::size_t capacity = 0;
   for(std::size_t r = 0; r != num_iterations; ++r){
      Container v;
      #ifdef BOOST_CONTAINER_VECTOR_ALLOC_STATS
         reset_alloc_stats(v);
      #endif
      //v.reserve(num_elements);
      //MyInt a[3];
/*
      for(std::size_t e = 0; e != num_elements/3; ++e){
         v.insert(v.end(), &a[0], &a[0]+3);
      }*/
/*
      for(std::size_t e = 0; e != num_elements/3; ++e){
         v.insert(v.end(), 3, MyInt((int)e));
      }*/
/*
      for(std::size_t e = 0; e != num_elements/3; ++e){
         v.insert(v.empty() ? v.end() : --v.end(), &a[0], &a[0]+3);
      }*/
/*
      for(std::size_t e = 0; e != num_elements/3; ++e){
         v.insert(v.empty() ? v.end() : --v.end(), 3, MyInt((int)e));
      }*/
/*
      for(std::size_t e = 0; e != num_elements/3; ++e){
         v.insert(v.size() >= 3 ? v.end()-3 : v.begin(), &a[0], &a[0]+3);
      }*/
/*
      for(std::size_t e = 0; e != num_elements/3; ++e){
         v.insert(v.size() >= 3 ? v.end()-3 : v.begin(), 3, MyInt((int)e));
      }*/
/*
      for(std::size_t e = 0; e != num_elements; ++e){
         v.insert(v.end(), MyInt((int)e));
      }*/
/*
      for(std::size_t e = 0; e != num_elements; ++e){
         v.insert(v.empty() ? v.end() : --v.end(), MyInt((int)e));
      }*/

      for(std::size_t e = 0; e != num_elements; ++e){
         v.push_back(MyInt((int)e));
      }

      #ifdef BOOST_CONTAINER_VECTOR_ALLOC_STATS
         numalloc  += get_num_alloc(v);
         numexpand += get_num_expand(v);
      #endif
      capacity = static_cast<std::size_t>(v.capacity());
   }

   timer.stop();
   nanosecond_type nseconds = timer.elapsed().wall;

   std::cout   << std::endl
               << "Allocator: " << typeid(typename Container::allocator_type).name()
               << std::endl
               << "  push_back ns:              "
               << float(nseconds)/float(num_iterations*num_elements)
               << std::endl
               << "  capacity  -  alloc calls (new/expand):  "
                  << (std::size_t)capacity << "  -  "
                  << (float(numalloc) + float(numexpand))/float(num_iterations)
                  << "(" << float(numalloc)/float(num_iterations) << "/" << float(numexpand)/float(num_iterations) << ")"
               << std::endl << std::endl;
   bc::dlmalloc_trim(0);
}

void print_header()
{
   std::cout   << "Allocator" << ";" << "Iterations" << ";" << "Size" << ";"
               << "Capacity" << ";" << "push_back(ns)" << ";" << "Allocator calls" << ";"
               << "New allocations" << ";" << "Fwd expansions" << std::endl;
}

int main()
{
   //#define SINGLE_TEST
   #define SIMPLE_IT
   #ifdef SINGLE_TEST
      #ifdef NDEBUG
      std::size_t numit [] = { 1000 };
      #else
      std::size_t numit [] = { 100 };
      #endif
      std::size_t numele [] = { 10000 };
   #elif defined SIMPLE_IT
      std::size_t numit [] = { 3 };
      std::size_t numele [] = { 10000 };
   #else
      #ifdef NDEBUG
      std::size_t numit []  = { 1000, 10000, 100000, 1000000 };
      #else
      std::size_t numit []  = { 100, 1000, 10000, 100000 };
      #endif
      std::size_t numele [] = { 10000, 1000,   100,     10       };
   #endif

   print_header();
   for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
      vector_test_template< bc::vector<MyInt, std::allocator<MyInt> > >(numit[i], numele[i]);
      vector_test_template< bc::vector<MyInt, bc::allocator<MyInt, 1> > >(numit[i], numele[i]);
      vector_test_template<bc::vector<MyInt, bc::allocator<MyInt, 2, bc::expand_bwd | bc::expand_fwd> > >(numit[i], numele[i]);
      vector_test_template<bc::vector<MyInt, bc::allocator<MyInt, 2> > >(numit[i], numele[i]);
   }
   return 0;
}
