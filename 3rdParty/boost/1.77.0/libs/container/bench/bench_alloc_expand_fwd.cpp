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
void vector_test_template(unsigned int num_iterations, unsigned int num_elements)
{
   unsigned int numalloc = 0, numexpand = 0;

   cpu_timer timer;
   timer.resume();

   unsigned int capacity = 0;
   for(unsigned int r = 0; r != num_iterations; ++r){
      Container v;
      #ifdef BOOST_CONTAINER_VECTOR_ALLOC_STATS
         reset_alloc_stats(v);
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
         numalloc  += get_num_alloc(v);
         numexpand += get_num_expand(v);
      #endif
      capacity = static_cast<unsigned int>(v.capacity());
   }

   timer.stop();
   nanosecond_type nseconds = timer.elapsed().wall;

   std::cout   << std::endl
               << "Allocator: " << typeid(typename Container::allocator_type).name()
               << std::endl
               << "  push_back ns:              "
               << float(nseconds)/(num_iterations*num_elements)
               << std::endl
               << "  capacity  -  alloc calls (new/expand):  "
                  << (unsigned int)capacity << "  -  "
                  << (float(numalloc) + float(numexpand))/num_iterations
                  << "(" << float(numalloc)/num_iterations << "/" << float(numexpand)/num_iterations << ")"
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
      unsigned int numit []  = { 1000, 10000, 100000, 1000000 };
      #else
      unsigned int numit []  = { 100, 1000, 10000, 100000 };
      #endif
      unsigned int numele [] = { 10000, 1000,   100,     10       };
   #endif

   print_header();
   for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
      vector_test_template< bc::vector<MyInt, std::allocator<MyInt> > >(numit[i], numele[i]);
      vector_test_template< bc::vector<MyInt, bc::allocator<MyInt, 1> > >(numit[i], numele[i]);
      vector_test_template<bc::vector<MyInt, bc::allocator<MyInt, 2, bc::expand_bwd | bc::expand_fwd> > >(numit[i], numele[i]);
      vector_test_template<bc::vector<MyInt, bc::allocator<MyInt, 2> > >(numit[i], numele[i]);
   }
   return 0;
}
