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
#include <iostream>  //std::cout

#include <boost/config.hpp>

#include <boost/move/unique_ptr.hpp>
#include <boost/move/algo/detail/merge_sort.hpp>

#include "order_type.hpp"
#include "random_shuffle.hpp"

#include <boost/move/algo/adaptive_merge.hpp>
#include <boost/move/core.hpp>
#include <cstdlib>


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
      ::random_shuffle(elements.get(), elements.get() + element_count);
      for(std::size_t j = 0; j < (num_keys ? num_keys : element_count); ++j){
         key_reps[j]=0;
      }
      for(std::size_t j = 0; j < element_count; ++j){
         elements[j].val = key_reps[elements[j].key]++;
      }

      boost::movelib::unique_ptr<char[]> buf(new char [sizeof(T)*(element_count-element_count/2)]);

      std::size_t const split = std::size_t(std::rand()) % element_count;
      boost::movelib::merge_sort(elements.get(), elements.get()+split, order_type_less(), (T*)buf.get());
      boost::movelib::merge_sort(elements.get()+split, elements.get()+element_count, order_type_less(), (T*)buf.get());
      
      boost::movelib::adaptive_merge(elements.get(), elements.get()+split, elements.get()+element_count, order_type_less());

      if (!is_order_type_ordered(elements.get(), element_count))
      {
         std::cout <<  "\n ERROR\n";
         std::abort();
      }
   }
   return true;
}

void instantiate_smalldiff_iterators()
{
   typedef randit<int, short> short_rand_it_t;
   boost::movelib::adaptive_merge(short_rand_it_t(), short_rand_it_t(), short_rand_it_t(), less_int());

   typedef randit<int, signed char> schar_rand_it_t;
   boost::movelib::adaptive_merge(schar_rand_it_t(), schar_rand_it_t(), schar_rand_it_t(), less_int());
}

int main()
{
   instantiate_smalldiff_iterators();

   const std::size_t NIter = 100;
   test_random_shuffled<order_move_type>(10001, 3,    NIter);
   test_random_shuffled<order_move_type>(10001, 65,   NIter);
   test_random_shuffled<order_move_type>(10001, 101,  NIter);
   test_random_shuffled<order_move_type>(10001, 1023, NIter);
   test_random_shuffled<order_move_type>(10001, 4095, NIter);
   test_random_shuffled<order_move_type>(10001, 0,    NIter);

   return 0;
}
