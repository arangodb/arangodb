//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <cstdlib>   //std::srand
#include <algorithm> //std::next_permutation
#include <iostream>  //std::cout

#include <boost/config.hpp>

#include <boost/move/unique_ptr.hpp>
#include <boost/container/vector.hpp>
#include <boost/timer/timer.hpp>

using boost::timer::cpu_timer;
using boost::timer::cpu_times;
using boost::timer::nanosecond_type;

#include "order_type.hpp"

#include <boost/move/algo/adaptive_merge.hpp>
#include <boost/move/core.hpp>


template<class T>
bool test_random_shuffled(std::size_t const element_count, std::size_t const num_keys, std::size_t const num_iter)
{
   boost::movelib::unique_ptr<T[]> elements(new T[element_count]);
   boost::movelib::unique_ptr<std::size_t[]> key_reps(new std::size_t[num_keys ? num_keys : element_count]);
   std::cout << "- - N: " << element_count << ", Keys: " << num_keys << ", It: " << num_iter << " \n";

   //Initialize keys
   for(std::size_t  i=0; i < element_count; ++i){
      std::size_t  key = num_keys ? (i % num_keys) : i;
      elements[i].key=key;
   }

   std::srand(0);

   for (std::size_t i = 0; i != num_iter; ++i)
   {
      std::random_shuffle(elements.get(), elements.get() + element_count);
      for(std::size_t i = 0; i < (num_keys ? num_keys : element_count); ++i){
         key_reps[i]=0;
      }
      for(std::size_t i = 0; i < element_count; ++i){
         elements[i].val = key_reps[elements[i].key]++;
      }

      boost::container::vector<order_type> tmp(elements.get(), elements.get()+element_count);
      std::size_t const split = std::size_t(std::rand()) % element_count;
      std::stable_sort(tmp.data(), tmp.data()+split, order_type_less<order_type>());
      std::stable_sort(tmp.data()+split, tmp.data()+element_count, order_type_less<order_type>());
      
      boost::movelib::adaptive_merge(tmp.data(), tmp.data()+split, tmp.data()+element_count, order_type_less<order_type>());

      if (!is_order_type_ordered(tmp.data(), element_count))
      {
         std::cout <<  "\n ERROR\n";
         throw int(0);
      }
   }
   return true;
}

int main()
{
   #ifdef NDEBUG
   const std::size_t NIter = 100;
   #else
   const std::size_t NIter = 10;
   #endif
   test_random_shuffled<order_type>(10001, 65,   NIter);
   test_random_shuffled<order_type>(10001, 101,  NIter);
   test_random_shuffled<order_type>(10001, 1023, NIter);
   test_random_shuffled<order_type>(10001, 4095, NIter);
   test_random_shuffled<order_type>(10001, 0,    NIter);

   return 0;
}
