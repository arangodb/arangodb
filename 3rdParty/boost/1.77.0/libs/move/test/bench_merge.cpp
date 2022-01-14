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

//#define BOOST_MOVE_ADAPTIVE_SORT_STATS
//#define BOOST_MOVE_ADAPTIVE_SORT_STATS_LEVEL 2

#include <algorithm> //std::inplace_merge
#include <cstdio>    //std::printf
#include <iostream>  //std::cout
#include <boost/container/vector.hpp>  //boost::container::vector

#include <boost/config.hpp>
#include <cstdlib>

#include <boost/move/unique_ptr.hpp>
#include <boost/move/detail/nsec_clock.hpp>

#include "order_type.hpp"
#include "random_shuffle.hpp"

using boost::move_detail::cpu_timer;
using boost::move_detail::nanosecond_type;

void print_stats(const char *str, boost::ulong_long_type element_count)
{
   std::printf("%sCmp:%8.04f Cpy:%9.04f\n", str, double(order_perf_type::num_compare)/element_count, double(order_perf_type::num_copy)/element_count );
}

#include <boost/move/algo/adaptive_merge.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/move/core.hpp>

template<class T, class Compare>
std::size_t generate_elements(boost::container::vector<T> &elements, std::size_t L, std::size_t NK, Compare comp)
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
   std::size_t split_count = L / 2;
   std::stable_sort(elements.data(), elements.data() + split_count, comp);
   std::stable_sort(elements.data() + split_count, elements.data() + L, comp);
   return split_count;
}

template<class T, class Compare>
void adaptive_merge_buffered(T *elements, T *mid, T *last, Compare comp, std::size_t BufLen)
{
   boost::movelib::unique_ptr<char[]> mem(new char[sizeof(T)*BufLen]);
   boost::movelib::adaptive_merge(elements, mid, last, comp, reinterpret_cast<T*>(mem.get()), BufLen);
}

template<class T, class Compare>
void std_like_adaptive_merge_buffered(T *elements, T *mid, T *last, Compare comp, std::size_t BufLen)
{
   boost::movelib::unique_ptr<char[]> mem(new char[sizeof(T)*BufLen]);
   boost::movelib::merge_adaptive_ONlogN(elements, mid, last, comp, reinterpret_cast<T*>(mem.get()), BufLen);
}

enum AlgoType
{
   StdMerge,
   AdaptMerge,
   SqrtHAdaptMerge,
   SqrtAdaptMerge,
   Sqrt2AdaptMerge,
   QuartAdaptMerge,
   StdInplaceMerge,
   StdSqrtHAdaptMerge,
   StdSqrtAdaptMerge,
   StdSqrt2AdaptMerge,
   StdQuartAdaptMerge,
   MaxMerge
};

const char *AlgoNames [] = { "StdMerge           "
                           , "AdaptMerge         "
                           , "SqrtHAdaptMerge    "
                           , "SqrtAdaptMerge     "
                           , "Sqrt2AdaptMerge    "
                           , "QuartAdaptMerge    "
                           , "StdInplaceMerge    "
                           , "StdSqrtHAdaptMerge "
                           , "StdSqrtAdaptMerge  "
                           , "StdSqrt2AdaptMerge "
                           , "StdQuartAdaptMerge "
                           };

BOOST_STATIC_ASSERT((sizeof(AlgoNames)/sizeof(*AlgoNames)) == MaxMerge);

template<class T>
bool measure_algo(T *elements, std::size_t element_count, std::size_t split_pos, std::size_t alg, nanosecond_type &prev_clock)
{
   std::printf("%s ", AlgoNames[alg]);
   order_perf_type::num_compare=0;
   order_perf_type::num_copy=0;
   order_perf_type::num_elements = element_count;
   cpu_timer timer;
   timer.resume();
   switch(alg)
   {
      case StdMerge:
         std::inplace_merge(elements, elements+split_pos, elements+element_count, order_type_less());
      break;
      case AdaptMerge:
         boost::movelib::adaptive_merge(elements, elements+split_pos, elements+element_count, order_type_less());
      break;
      case SqrtHAdaptMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count)/2+1);
      break;
      case SqrtAdaptMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case Sqrt2AdaptMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , 2*boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case QuartAdaptMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , (element_count)/4+1);
      break;
      case StdInplaceMerge:
         boost::movelib::merge_bufferless_ONlogN(elements, elements+split_pos, elements+element_count, order_type_less());
      break;
      case StdSqrtHAdaptMerge:
         std_like_adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count)/2+1);
      break;
      case StdSqrtAdaptMerge:
         std_like_adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case StdSqrt2AdaptMerge:
         std_like_adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , 2*boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case StdQuartAdaptMerge:
         std_like_adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, order_type_less()
                            , (element_count)/4+1);
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
   std::printf("Cmp:%8.04f Cpy:%9.04f", double(order_perf_type::num_compare)/element_count, double(order_perf_type::num_copy)/element_count );

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
   bool res = is_order_type_ordered(elements, element_count, true);
   return res;
}

template<class T>
bool measure_all(std::size_t L, std::size_t NK)
{
   boost::container::vector<T> original_elements, elements;
   std::size_t split_pos = generate_elements(original_elements, L, NK, order_type_less());
   std::printf("\n - - N: %u, NK: %u - -\n", (unsigned)L, (unsigned)NK);

   nanosecond_type prev_clock = 0;
   nanosecond_type back_clock;
   bool res = true;

   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, StdMerge, prev_clock);
   back_clock = prev_clock;
   //

   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, QuartAdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, StdQuartAdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, Sqrt2AdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, StdSqrt2AdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, SqrtAdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, StdSqrtAdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, SqrtHAdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, StdSqrtHAdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos, AdaptMerge, prev_clock);
   //
   prev_clock = back_clock;
   elements = original_elements;
   res = res && measure_algo(elements.data(), L, split_pos,StdInplaceMerge, prev_clock);
   //
   if (!res)
      std::abort();
   return res;
}

//Undef it to run the long test
#define BENCH_MERGE_SHORT
#define BENCH_SORT_UNIQUE_VALUES

int main()
{
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(101,1);
   measure_all<order_perf_type>(101,5);
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
   #if defined(NDEBUG)
   #ifndef BENCH_SORT_UNIQUE_VALUES
   measure_all<order_perf_type>(100001,511);
   measure_all<order_perf_type>(100001,2047);
   measure_all<order_perf_type>(100001,8191);
   measure_all<order_perf_type>(100001,32767);
   #endif
   measure_all<order_perf_type>(100001,0);

   //
   #if !defined(BENCH_MERGE_SHORT)
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
   measure_all<order_perf_type>(10000001,0);
   #endif   //#ifndef BENCH_MERGE_SHORT
   #endif   //#ifdef NDEBUG

   return 0;
}

