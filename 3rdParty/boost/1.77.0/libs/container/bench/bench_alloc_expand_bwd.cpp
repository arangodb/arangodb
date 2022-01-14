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
#endif

#include <boost/container/allocator.hpp>
#include <boost/core/no_exceptions_support.hpp>

#define BOOST_CONTAINER_VECTOR_ALLOC_STATS

#include <boost/container/vector.hpp>
#include <memory>    //std::allocator
#include <iostream>  //std::cout, std::endl
#include <cassert>   //assert

#include <boost/move/detail/nsec_clock.hpp>

using boost::move_detail::cpu_timer;
using boost::move_detail::cpu_times;
using boost::move_detail::nanosecond_type;

//typedef int MyInt;

class MyInt
{
   int int_;

   public:
   MyInt(int i = 0)
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
namespace boost{

template<class T>
struct has_trivial_destructor_after_move;

template<>
struct has_trivial_destructor_after_move<MyInt>
{
   static const bool value = true;
   //static const bool value = false;
};

}  //namespace boost{


namespace bc = boost::container;

typedef std::allocator<MyInt>   StdAllocator;
typedef bc::allocator<MyInt, 2, bc::expand_bwd | bc::expand_fwd> AllocatorPlusV2Mask;
typedef bc::allocator<MyInt, 2, bc::expand_fwd> AllocatorPlusV2;
typedef bc::allocator<MyInt, 1> AllocatorPlusV1;

template<class Allocator> struct get_allocator_name;

template<> struct get_allocator_name<StdAllocator>
{  static const char *get() {  return "StdAllocator";  } };

template<> struct get_allocator_name<AllocatorPlusV2Mask>
{  static const char *get() {  return "AllocatorPlusV2Mask";  }   };

template<> struct get_allocator_name<AllocatorPlusV2>
{  static const char *get() {  return "AllocatorPlusV2";  } };

template<> struct get_allocator_name<AllocatorPlusV1>
{  static const char *get() {  return "AllocatorPlusV1";  } };




void print_header()
{
   std::cout   << "Allocator" << ";" << "Iterations" << ";" << "Size" << ";"
               << "Capacity" << ";" << "push_back(ns)" << ";" << "Allocator calls" << ";"
               << "New allocations" << ";" << "Bwd expansions" << std::endl;
}

template<class Allocator>
void vector_test_template(unsigned int num_iterations, unsigned int num_elements, bool csv_output)
{
   typedef Allocator IntAllocator;
   unsigned int numalloc = 0, numexpand = 0;

   cpu_timer timer;
   timer.resume();

   unsigned int capacity = 0;
   for(unsigned int r = 0; r != num_iterations; ++r){
      bc::vector<MyInt, IntAllocator> v;
      v.reset_alloc_stats();
      void *first_mem = 0;
      BOOST_TRY{
         first_mem = bc::dlmalloc_malloc(sizeof(MyInt)*num_elements*3/2);
         v.push_back(MyInt(0));
         bc::dlmalloc_free(first_mem);

         for(unsigned int e = 0; e != num_elements; ++e){
            v.push_back(MyInt(e));
         }
         numalloc  += v.num_alloc;
         numexpand += v.num_expand_bwd;
         capacity = static_cast<unsigned int>(v.capacity());
      }
      BOOST_CATCH(...){
         bc::dlmalloc_free(first_mem);
         BOOST_RETHROW;
      }
      BOOST_CATCH_END
   }

   assert(bc::dlmalloc_allocated_memory() == 0);

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
                  << std::endl;
      std::cout   << '\n'
                  << "    -----------------------------------    "
                  << std::endl;
   }
   bc::dlmalloc_trim(0);
}

int main(int argc, const char *argv[])
{
   //#define SINGLE_TEST
   #define SIMPLE_IT
   #ifdef SINGLE_TEST
      #ifdef NDEBUG
      unsigned int numit [] = { 10 };
      #else
      unsigned int numit [] = { 10 };
      #endif
      unsigned int numele [] = { 10000 };
   #elif defined(SIMPLE_IT)
      unsigned int numit [] = { 3 };
      unsigned int numele[] = { 10000 };
   #else
      #ifdef NDEBUG
      unsigned int numit [] = { 2000, 20000, 200000, 2000000 };
      #else
      unsigned int numit [] = { 100, 1000, 10000, 100000 };
      #endif
      unsigned int numele [] = { 10000, 1000,   100,     10       };
   #endif

   bool csv_output = argc == 2 && (strcmp(argv[1], "--csv-output") == 0);

   if(csv_output){
      print_header();
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<StdAllocator>(numit[i], numele[i], csv_output);
      }
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV1>(numit[i], numele[i], csv_output);
      }
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV2Mask>(numit[i], numele[i], csv_output);
      }
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV2>(numit[i], numele[i], csv_output);
      }
   }
   else{
      for(unsigned int i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         std::cout   << "\n    -----------------------------------    \n"
                     <<   "  Iterations/Elements:         " << numit[i] << "/" << numele[i]
                     << "\n    -----------------------------------    \n";
         vector_test_template<StdAllocator>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV1>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV2Mask>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV2>(numit[i], numele[i], csv_output);
      }
   }
   return 0;
}
