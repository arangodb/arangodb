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

#define BOOST_CONTAINER_VECTOR_ALLOC_STATS

#include <boost/container/vector.hpp>

#undef BOOST_CONTAINER_VECTOR_ALLOC_STATS

#include <memory>    //std::allocator
#include <iostream>  //std::cout, std::endl
#include <cassert>   //assert

#include <boost/move/detail/nsec_clock.hpp>
using boost::move_detail::cpu_timer;
using boost::move_detail::cpu_times;
using boost::move_detail::nanosecond_type;

namespace bc = boost::container;

class MyInt
{
   std::ptrdiff_t int_; //Use a type that will grow on 64 bit machines

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
typedef bc::allocator<MyInt, 2> AllocatorPlusV2;
typedef bc::allocator<MyInt, 1> AllocatorPlusV1;

template<class Allocator> struct get_allocator_name;

template<> struct get_allocator_name<StdAllocator>
{  static const char *get() {  return "StdAllocator";  } };

template<> struct get_allocator_name<AllocatorPlusV2>
{  static const char *get() {  return "AllocatorPlusV2";  } };

template<> struct get_allocator_name<AllocatorPlusV1>
{  static const char *get() {  return "AllocatorPlusV1";  } };

void print_header()
{
   std::cout   << "Allocator" << ";" << "Iterations" << ";" << "Size" << ";"
               << "num_shrink" << ";" << "shrink_to_fit(ns)" << std::endl;
}

template<class Allocator>
void vector_test_template(std::size_t num_iterations, std::size_t num_elements, bool csv_output)
{
   typedef Allocator IntAllocator;

   std::size_t capacity = 0;
   const std::size_t Step = 5;
   std::size_t num_shrink = 0;
   (void)capacity;

   cpu_timer timer;
   timer.resume();

   #ifndef NDEBUG
   typedef bc::dtl::integral_constant
      <unsigned, bc::dtl::version<Allocator>::value> alloc_version;
   #endif

   for(std::size_t r = 0; r != num_iterations; ++r){
      bc::vector<MyInt, IntAllocator> v(num_elements);
      v.reset_alloc_stats();
      num_shrink = 0;
      for(std::size_t e = num_elements; e != 0; e -= Step){
         v.erase(v.end() - std::ptrdiff_t(Step), v.end());
         v.shrink_to_fit();
         assert( (alloc_version::value != 2) || (e == Step) || (v.num_shrink > num_shrink) );
         num_shrink = v.num_shrink;
      }
      assert(v.empty());
      assert(0 == v.capacity());
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
                  << num_shrink
                  << ";"
                  << float(nseconds)/float(num_iterations*num_elements)
                  << std::endl;
   }
   else{
      std::cout   << std::endl
                  << "Allocator: " << get_allocator_name<Allocator>::get()
                  << std::endl
                  << "  num_shrink:         " << num_shrink
                  << std::endl
                  << "  shrink_to_fit ns:   "
                  << float(nseconds)/float(num_iterations*num_elements)
                  << std::endl << std::endl;
   }
   bc::dlmalloc_trim(0);
}

int main(int argc, const char *argv[])
{
   //#define SINGLE_TEST
   #define SIMPLE_IT
   #ifdef SINGLE_TEST
      #ifdef NDEBUG
      std::size_t numit [] =  { 10 };
      #else
      std::size_t numit [] =  { 50 };
      std::size_t numele[] = { 2000 };
      #endif
   #elif defined SIMPLE_IT
      std::size_t numit [] =  { 3 };
      std::size_t numele[] = { 2000 };
   #else
      #ifdef NDEBUG
      std::size_t numit [] =  { 100,   1000, 10000 };
      #else
      std::size_t numit [] =  { 10,   100, 1000 };
      #endif
      std::size_t numele [] = { 10000, 2000, 500   };
   #endif

   bool csv_output = argc == 2 && (strcmp(argv[1], "--csv-output") == 0);

   if(csv_output){
      print_header();
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<StdAllocator>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV1>(numit[i], numele[i], csv_output);
      }
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         vector_test_template<AllocatorPlusV2>(numit[i], numele[i], csv_output);
      }
   }
   else{
      for(std::size_t i = 0; i < sizeof(numele)/sizeof(numele[0]); ++i){
         std::cout   << "\n    -----------------------------------    \n"
                     <<   "  Iterations/Elements:         " << numit[i] << "/" << numele[i]
                     << "\n    -----------------------------------    \n";
         vector_test_template<StdAllocator>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV1>(numit[i], numele[i], csv_output);
         vector_test_template<AllocatorPlusV2>(numit[i], numele[i], csv_output);
      }
   }
   return 0;
}
