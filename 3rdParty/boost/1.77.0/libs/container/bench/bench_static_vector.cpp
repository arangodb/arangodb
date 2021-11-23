// benchmark based on: http://cpp-next.com/archive/2010/10/howards-stl-move-semantics-benchmark/
//
//  @file   bench_static_vector.cpp
//  @date   Aug 14, 2011
//  @author Andrew Hundt <ATHundt@gmail.com>
//
//  (C) Copyright 2011-2013 Andrew Hundt <ATHundt@gmail.com>
//  (C) Copyright 2013-2013 Ion Gaztanaga
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  @brief  varray_benchmark.cpp compares the performance of boost::container::varray to boost::container::vector

#include "varray.hpp"
#include <boost/container/vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/core/no_exceptions_support.hpp>

#include "../test/movable_int.hpp"
#include <vector>
#include <iostream>
#include <boost/move/detail/nsec_clock.hpp>
#include <algorithm>
#include <exception>
#include <iomanip>

using boost::move_detail::cpu_timer;
using boost::move_detail::cpu_times;
using boost::move_detail::nanosecond_type;

static const std::size_t N = 500;

#ifdef NDEBUG
static const std::size_t Iter = 50;
#else
static const std::size_t Iter = 5;
#endif

//#define BENCH_SIMPLE_CONSTRUCTION
//#define BENCH_TRIVIAL_TYPE

#ifdef BENCH_TRIVIAL_TYPE
typedef std::size_t basic_type_t;
#else
typedef boost::container::test::copyable_int basic_type_t;
#endif

template<typename T>
T &get_set(std::size_t)
{
   #ifdef BENCH_SIMPLE_CONSTRUCTION
   T &t = *new T(N);
   for (std::size_t i = 0; i < N; ++i)
      t[i] = basic_type_t(std::rand());
   #else
   T &t = *new T;
   t.reserve(N);
   for (std::size_t i = 0; i < N; ++i)
      t.push_back(basic_type_t(std::rand()));
   #endif
   return t;
}

template<typename T>
T &generate()
{
   T &v = *new T;
   v.reserve(N);

   for (std::size_t i = 0; i < N; ++i){
      typename T::reference r = get_set<typename T::value_type>(i);
      v.push_back(boost::move(r));
      delete &r;
   }
   return v;
}

template<typename T>
cpu_times time_it()
{
   cpu_timer sortTime,rotateTime,destructionTime;
   sortTime.stop();rotateTime.stop();destructionTime.stop();
   cpu_timer totalTime, constructTime;
   std::srand (0);
   for(std::size_t i = 0; i< Iter; ++i){
     constructTime.resume();
     {
      T &v = generate<T>();
      constructTime.stop();
      sortTime.resume();
      std::sort(v.begin(), v.end());
      sortTime.stop();
      rotateTime.resume();
      std::rotate(v.begin(), v.begin() + v.size()/2, v.end());
      rotateTime.stop();
      destructionTime.resume();
      delete &v;
     }
     destructionTime.stop();
   }
   totalTime.stop();
   std::cout << std::fixed << std::setw( 11 );
   std::cout << "  construction took " << double(constructTime.elapsed().wall)/double(1000000000) << "s\n";
   std::cout << "  sort took         " << double(sortTime.elapsed().wall)/double(1000000000) << "s\n";
   std::cout << "  rotate took       " << double(rotateTime.elapsed().wall)/double(1000000000) << "s\n";
   std::cout << "  destruction took  " << double(destructionTime.elapsed().wall)/double(1000000000) << "s\n";
   std::cout << "  Total time =      " << double(totalTime.elapsed().wall)/double(1000000000) << "s\n";
   return totalTime.elapsed();
}

void compare_times(cpu_times time_numerator, cpu_times time_denominator){
   std::cout
   << "\n  wall       = " << ((double)time_numerator.wall/(double)time_denominator.wall) << "\n\n";
}

int main()
{
   std::cout << "N = " << N << " Iter = " << Iter << "\n\n";

   std::cout << "varray benchmark:" << std::endl;
   cpu_times time_varray = time_it<boost::container::varray<boost::container::varray<basic_type_t,N>,N > >();

   std::cout << "boost::container::static_vector benchmark" << std::endl;
   cpu_times time_boost_static_vector = time_it<boost::container::static_vector<boost::container::static_vector<basic_type_t,N>,N > >();

   std::cout << "boost::container::vector benchmark"  << std::endl;
   cpu_times time_boost_vector = time_it<boost::container::vector<boost::container::vector<basic_type_t> > >();

   std::cout << "std::vector benchmark" << std::endl;
   cpu_times time_standard_vector = time_it<std::vector<std::vector<basic_type_t> > >();

   std::cout << "varray/boost::container::vector total time comparison:";
   compare_times(time_varray, time_boost_vector);

   std::cout << "varray/boost::container::static_vector total time comparison:";
   compare_times(time_varray, time_boost_static_vector);

   std::cout << "varray/std::vector total time comparison:";
   compare_times(time_varray,time_standard_vector);

   return 0;
}
