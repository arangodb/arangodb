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
#include <boost/container/vector.hpp>  //boost::container::vector

#include <boost/config.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/move/detail/nsec_clock.hpp>
#include <cstdlib>

using boost::move_detail::cpu_timer;
using boost::move_detail::nanosecond_type;

#include "order_type.hpp"
#include "random_shuffle.hpp"

//#define BOOST_MOVE_ADAPTIVE_SORT_STATS
//#define BOOST_MOVE_ADAPTIVE_SORT_INVARIANTS
void print_stats(const char *str, boost::ulong_long_type element_count)
{
   std::printf( "%sCmp:%7.03f Cpy:%8.03f\n", str
              , double(order_perf_type::num_compare)/double(element_count)
              , double(order_perf_type::num_copy)/double(element_count) );
}


#include <boost/move/algo/adaptive_sort.hpp>
#include <boost/move/algo/detail/merge_sort.hpp>
#include <boost/move/algo/detail/pdqsort.hpp>
#include <boost/move/algo/detail/heap_sort.hpp>
#include <boost/move/core.hpp>

template<class T>
void generate_elements(boost::container::vector<T> &elements, std::size_t L, std::size_t NK)
{
   elements.resize(L);
   boost::movelib::unique_ptr<std::size_t[]> key_reps(new std::size_t[NK ? NK : L]);

   std::srand(0);
   for (std::size_t i = 0; i < (NK ? NK : L); ++i) {
      key_reps[i] = 0;
   }
   for (std::size_t i = 0; i < L; ++i) {
      std::size_t  key = NK ? (i % NK) : i;
      elements[i].key = key;
   }
   ::random_shuffle(elements.data(), elements.data() + L);
   ::random_shuffle(elements.data(), elements.data() + L);

   for (std::size_t i = 0; i < L; ++i) {
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
void std_like_adaptive_stable_sort_buffered(T *elements, std::size_t element_count, Compare comp, std::size_t BufLen)
{
   boost::movelib::unique_ptr<char[]> mem(new char[sizeof(T)*BufLen]);
   boost::movelib::stable_sort_adaptive_ONlogN2(elements, elements + element_count, comp, reinterpret_cast<T*>(mem.get()), BufLen);
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
   StdSqrtHAdpSort,
   StdSqrtAdpSort,
   StdSqrt2AdpSort,
   StdQuartAdpSort,
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
                           , "StdSqrtHAdpSort"
                           , "StdSqrtAdpSort "
                           , "StdSqrt2AdpSort"
                           , "StdQuartAdpSort"
                           , "SlowSort       "
                           , "HeapSort       "
                           };

BOOST_STATIC_ASSERT((sizeof(AlgoNames)/sizeof(*AlgoNames)) == MaxSort);

template<class T>
bool measure_algo(T *elements, std::size_t element_count, std::size_t alg, nanosecond_type &prev_clock)
{
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
      case StdSqrtHAdpSort:
         std_like_adaptive_stable_sort_buffered( elements, element_count, order_type_less()
                                              , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count)/2+1);
      break;
      case StdSqrtAdpSort:
         std_like_adaptive_stable_sort_buffered( elements, element_count, order_type_less()
                                               , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case StdSqrt2AdpSort:
         std_like_adaptive_stable_sort_buffered( elements, element_count, order_type_less()
                                               , 2*boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case StdQuartAdpSort:
         std_like_adaptive_stable_sort_buffered( elements, element_count, order_type_less()
                                               , (element_count-1)/4+1);
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
   std::printf("Cmp:%7.03f Cpy:%8.03f", double(order_perf_type::num_compare)/double(element_count), double(order_perf_type::num_copy)/double(element_count) );

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
   boost::container::vector<T> original_elements, elements;
   generate_elements(original_elements, L, NK);
   std::printf("\n - - N: %u, NK: %u - -\n", (unsigned)L, (unsigned)NK);

   nanosecond_type prev_clock = 0;
   nanosecond_type back_clock;
   bool res = true;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,MergeSort, prev_clock);
   back_clock = prev_clock;
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,StableSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,PdQsort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,StdSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,HeapSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,QuartAdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, StdQuartAdpSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,Sqrt2AdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, StdSqrt2AdpSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,SqrtAdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, StdSqrtAdpSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,SqrtHAdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, StdSqrtHAdpSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,AdaptiveSort, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L,InplaceStableSort, prev_clock);
   //
   //prev_clock = back_clock;
   //elements = original_elements;
   //res = res && measure_algo(elements.data(), L,SlowStableSort, prev_clock);

   if(!res)
      std::abort();
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
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(10001,65);
   measure_all<order_perf_type>(10001,255);
   measure_all<order_perf_type>(10001,1023);
   measure_all<order_perf_type>(10001,4095);
   #endif
   measure_all<order_perf_type>(10001,0);

   //
   #ifdef NDEBUG
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(100001,511);
   measure_all<order_perf_type>(100001,2047);
   measure_all<order_perf_type>(100001,8191);
   measure_all<order_perf_type>(100001,32767);
   #endif
   measure_all<order_perf_type>(100001,0);

   //
   #ifndef BENCH_SORT_SHORT
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(1000001, 8192);
   measure_all<order_perf_type>(1000001, 32768);
   measure_all<order_perf_type>(1000001, 131072);
   measure_all<order_perf_type>(1000001, 524288);
   #endif
   measure_all<order_perf_type>(1000001,0);

   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(10000001, 65536);
   measure_all<order_perf_type>(10000001, 262144);
   measure_all<order_perf_type>(10000001, 1048576);
   measure_all<order_perf_type>(10000001, 4194304);
   #endif
   measure_all<order_perf_type>(1000001,0);
   #endif   //#ifndef BENCH_SORT_SHORT
   #endif   //NDEBUG

   //measure_all<order_perf_type>(100000001,0);

   return 0;
}
