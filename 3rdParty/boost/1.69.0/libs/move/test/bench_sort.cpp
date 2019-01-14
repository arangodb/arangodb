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
#include <algorithm> //std::stable_sort, std::make|sort_heap, std::random_shuffle
#include <cstdio>    //std::printf
#include <iostream>  //std::cout

#include <boost/config.hpp>

#include <boost/move/unique_ptr.hpp>
#include <boost/timer/timer.hpp>

using boost::timer::cpu_timer;
using boost::timer::cpu_times;
using boost::timer::nanosecond_type;

#include "order_type.hpp"
#include "random_shuffle.hpp"

//#define BOOST_MOVE_ADAPTIVE_SORT_STATS
//#define BOOST_MOVE_ADAPTIVE_SORT_INVARIANTS
void print_stats(const char *str, boost::ulong_long_type element_count)
{
   std::printf("%sCmp:%7.03f Cpy:%8.03f\n", str, double(order_perf_type::num_compare)/element_count, double(order_perf_type::num_copy)/element_count );
}


#include <boost/move/algo/adaptive_sort.hpp>
#include <boost/move/algo/detail/merge_sort.hpp>
#include <boost/move/algo/detail/pdqsort.hpp>
#include <boost/move/algo/detail/heap_sort.hpp>
#include <boost/move/core.hpp>

template<class T>
void generate_elements(T elements[], std::size_t element_count, std::size_t key_reps[], std::size_t key_len)
{
   std::srand(0);
   for(std::size_t i = 0; i < (key_len ? key_len : element_count); ++i){
      key_reps[i]=0;
   }
   for(std::size_t  i=0; i < element_count; ++i){
      std::size_t  key = key_len ? (i % key_len) : i;
      elements[i].key=key;
   }
   ::random_shuffle(elements, elements + element_count);
   ::random_shuffle(elements, elements + element_count);
   ::random_shuffle(elements, elements + element_count);
   for(std::size_t i = 0; i < element_count; ++i){
      elements[i].val = key_reps[elements[i].key]++;
   }
}

template<class T, class Compare>
void adaptive_sort_buffered(T *elements, std::size_t element_count, Compare comp, std::size_t BufLen)
{
   boost::movelib::unique_ptr<char[]> mem(new char[sizeof(T)*BufLen]);
   boost::movelib::adaptive_sort(elements, elements + element_count, comp, reinterpret_cast<T*>(mem.get()), BufLen);
}

template<class T, class Compare>
void merge_sort_buffered(T *elements, std::size_t element_count, Compare comp)
{
   boost::movelib::unique_ptr<char[]> mem(new char[sizeof(T)*((element_count+1)/2)]);
   boost::movelib::merge_sort(elements, elements + element_count, comp, reinterpret_cast<T*>(mem.get()));
}

enum AlgoType
{
   MergeSort,
   StableSort,
   PdQsort,
   StdSort,
   AdaptiveSort,
   SqrtHAdaptiveSort,
   SqrtAdaptiveSort,
   Sqrt2AdaptiveSort,
   QuartAdaptiveSort,
   InplaceStableSort,
   SlowStableSort,
   HeapSort,
   MaxSort
};

const char *AlgoNames [] = { "MergeSort      "
                           , "StableSort     "
                           , "PdQsort        "
                           , "StdSort        "
                           , "AdaptSort      "
                           , "SqrtHAdaptSort "
                           , "SqrtAdaptSort  "
                           , "Sqrt2AdaptSort "
                           , "QuartAdaptSort "
                           , "InplStableSort "
                           , "SlowSort       "
                           , "HeapSort       "
                           };

BOOST_STATIC_ASSERT((sizeof(AlgoNames)/sizeof(*AlgoNames)) == MaxSort);

template<class T>
bool measure_algo(T *elements, std::size_t key_reps[], std::size_t element_count, std::size_t key_len, unsigned alg, nanosecond_type &prev_clock)
{
   generate_elements(elements, element_count, key_reps, key_len);

   std::printf("%s ", AlgoNames[alg]);
   order_perf_type::num_compare=0;
   order_perf_type::num_copy=0;
   order_perf_type::num_elements = element_count;
   cpu_timer timer;
   timer.resume();
   switch(alg)
   {
      case MergeSort:
         merge_sort_buffered(elements, element_count, order_type_less());
      break;
      case StableSort:
         std::stable_sort(elements,elements+element_count,order_type_less());
      break;
      case PdQsort:
         boost::movelib::pdqsort(elements,elements+element_count,order_type_less());
      break;
      case StdSort:
         std::sort(elements,elements+element_count,order_type_less());
      break;
      case AdaptiveSort:
         boost::movelib::adaptive_sort(elements, elements+element_count, order_type_less());
      break;
      case SqrtHAdaptiveSort:
         adaptive_sort_buffered( elements, element_count, order_type_less()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count)/2+1);
      break;
      case SqrtAdaptiveSort:
         adaptive_sort_buffered( elements, element_count, order_type_less()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case Sqrt2AdaptiveSort:
         adaptive_sort_buffered( elements, element_count, order_type_less()
                            , 2*boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case QuartAdaptiveSort:
         adaptive_sort_buffered( elements, element_count, order_type_less()
                            , (element_count-1)/4+1);
      break;
      case InplaceStableSort:
         boost::movelib::inplace_stable_sort(elements, elements+element_count, order_type_less());
      break;
      case SlowStableSort:
         boost::movelib::detail_adaptive::slow_stable_sort(elements, elements+element_count, order_type_less());
      break;
      case HeapSort:
         boost::movelib::heap_sort(elements, elements+element_count, order_type_less());
         boost::movelib::heap_sort((order_move_type*)0, (order_move_type*)0, order_type_less());

      break;
   }
   timer.stop();

   if(order_perf_type::num_elements == element_count){
      std::printf(" Tmp Ok ");
   } else{
      std::printf(" Tmp KO ");
   }
   nanosecond_type new_clock = timer.elapsed().wall;

   //std::cout << "Cmp:" << order_perf_type::num_compare << " Cpy:" << order_perf_type::num_copy;   //for old compilers without ll size argument
   std::printf("Cmp:%7.03f Cpy:%8.03f", double(order_perf_type::num_compare)/element_count, double(order_perf_type::num_copy)/element_count );

   double time = double(new_clock);

   const char *units = "ns";
   if(time >= 1000000000.0){
      time /= 1000000000.0;
      units = " s";
   }
   else if(time >= 1000000.0){
      time /= 1000000.0;
      units = "ms";
   }
   else if(time >= 1000.0){
      time /= 1000.0;
      units = "us";
   }

   std::printf(" %6.02f%s (%6.02f)\n"
              , time
              , units
              , prev_clock ? double(new_clock)/double(prev_clock): 1.0);
   prev_clock = new_clock;
   bool res = is_order_type_ordered(elements, element_count, alg != HeapSort && alg != PdQsort && alg != StdSort);
   return res;
}

template<class T>
bool measure_all(std::size_t L, std::size_t NK)
{
   boost::movelib::unique_ptr<T[]> pdata(new T[L]);
   boost::movelib::unique_ptr<std::size_t[]> pkeys(new std::size_t[NK ? NK : L]);
   T *A              = pdata.get();
   std::size_t *Keys = pkeys.get();
   std::printf("\n - - N: %u, NK: %u - -\n", (unsigned)L, (unsigned)NK);

   nanosecond_type prev_clock = 0;
   nanosecond_type back_clock;
   bool res = true;
   res = res && measure_algo(A,Keys,L,NK,MergeSort, prev_clock);
   back_clock = prev_clock;
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,StableSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,PdQsort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,StdSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,HeapSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,QuartAdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,Sqrt2AdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,SqrtAdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,SqrtHAdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,AdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,InplaceStableSort, prev_clock);
   //
   //prev_clock = back_clock;
   //res = res && measure_algo(A,Keys,L,NK,SlowStableSort, prev_clock);
   //
   if(!res)
      throw int(0);
   return res;
}

//Undef it to run the long test
#define BENCH_SORT_SHORT
#define BENCH_SORT_UNIQUE_VALUES

int main()
{
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(101,1);
   measure_all<order_perf_type>(101,7);
   measure_all<order_perf_type>(101,31);
   #endif
   measure_all<order_perf_type>(101,0);

   //
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(1101,1);
   measure_all<order_perf_type>(1001,7);
   measure_all<order_perf_type>(1001,31);
   measure_all<order_perf_type>(1001,127);
   measure_all<order_perf_type>(1001,511);
   #endif
   measure_all<order_perf_type>(1001,0);
   //
   #ifndef BENCH_SORT_SHORT
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(10001,65);
   measure_all<order_perf_type>(10001,255);
   measure_all<order_perf_type>(10001,1023);
   measure_all<order_perf_type>(10001,4095);
   #endif
   measure_all<order_perf_type>(10001,0);

   //
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(100001,511);
   measure_all<order_perf_type>(100001,2047);
   measure_all<order_perf_type>(100001,8191);
   measure_all<order_perf_type>(100001,32767);
   #endif
   measure_all<order_perf_type>(100001,0);

   //
   #ifdef NDEBUG
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(1000001,1);
   measure_all<order_perf_type>(1000001,1024);
   measure_all<order_perf_type>(1000001,32768);
   measure_all<order_perf_type>(1000001,524287);
   #endif
   measure_all<order_perf_type>(1000001,0);
   measure_all<order_perf_type>(1500001,0);
   #endif   //NDEBUG

   #endif   //#ifndef BENCH_SORT_SHORT

   //measure_all<order_perf_type>(100000001,0);

   return 0;
}
