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

#include <algorithm> //std::inplace_merge
#include <cstdio>    //std::printf
#include <iostream>  //std::cout

#include <boost/config.hpp>

#include <boost/move/unique_ptr.hpp>
#include <boost/timer/timer.hpp>

using boost::timer::cpu_timer;
using boost::timer::cpu_times;
using boost::timer::nanosecond_type;


boost::ulong_long_type num_copy;
boost::ulong_long_type num_elements;

struct merged_type
{
   public:
   std::size_t key;
   std::size_t val;

   merged_type()
   {
      ++num_elements;
   }

   merged_type(const merged_type& other)
      : key(other.key), val(other.val)
   {
      ++num_elements;
      ++num_copy;
   }

   merged_type & operator=(const merged_type& other)
   {
      ++num_copy;
      key = other.key;
      val = other.val;
      return *this;
   }

   ~merged_type ()
   {
      --num_elements;
   }
};

boost::ulong_long_type num_compare;

//#define BOOST_MOVE_ADAPTIVE_SORT_STATS
void print_stats(const char *str, boost::ulong_long_type element_count)
{
   std::printf("%sCmp:%8.04f Cpy:%9.04f\n", str, double(num_compare)/element_count, double(num_copy)/element_count );
}


template<class T>
struct counted_less
{
   bool operator()(const T &a,T const &b) const
   {  ++num_compare; return a.key < b.key;   }
};

#include <boost/move/algo/adaptive_merge.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/move/core.hpp>

template<class T, class Compare>
std::size_t generate_elements(T elements[], std::size_t element_count, std::size_t key_reps[], std::size_t key_len, Compare comp)
{
   std::srand(0);
   for(std::size_t i = 0; i < (key_len ? key_len : element_count); ++i){
      key_reps[i]=0;
   }
   for(std::size_t  i=0; i < element_count; ++i){
      std::size_t  key = key_len ? (i % key_len) : i;
      elements[i].key=key;
   }
   std::random_shuffle(elements, elements + element_count);
   std::random_shuffle(elements, elements + element_count);
   std::random_shuffle(elements, elements + element_count);
   for(std::size_t i = 0; i < element_count; ++i){
      elements[i].val = key_reps[elements[i].key]++;
   }
   std::size_t split_count = element_count/2;
   std::stable_sort(elements, elements+split_count, comp);
   std::stable_sort(elements+split_count, elements+element_count, comp);
   return split_count;
}

template<class T>
bool test_order(T *elements, std::size_t element_count, bool stable = true)
{
   for(std::size_t i = 1; i < element_count; ++i){
      if(counted_less<T>()(elements[i], elements[i-1])){
         std::printf("\n Ord KO !!!!");
         return false;
      }
      if( stable && !(counted_less<T>()(elements[i-1], elements[i])) && (elements[i-1].val > elements[i].val) ){
         std::printf("\n Stb KO !!!! ");
         return false;
      }
   }
   return true;
}

template<class T, class Compare>
void adaptive_merge_buffered(T *elements, T *mid, T *last, Compare comp, std::size_t BufLen)
{
   boost::movelib::unique_ptr<char[]> mem(new char[sizeof(T)*BufLen]);
   boost::movelib::adaptive_merge(elements, mid, last, comp, reinterpret_cast<T*>(mem.get()), BufLen);
}

enum AlgoType
{
   InplaceMerge,
   AdaptiveMerge,
   SqrtHAdaptiveMerge,
   SqrtAdaptiveMerge,
   Sqrt2AdaptiveMerge,
   QuartAdaptiveMerge,
   BuflessMerge,
   MaxMerge
};

const char *AlgoNames [] = { "InplaceMerge    "
                           , "AdaptMerge      "
                           , "SqrtHAdaptMerge "
                           , "SqrtAdaptMerge  "
                           , "Sqrt2AdaptMerge "
                           , "QuartAdaptMerge "
                           , "BuflessMerge    "
                           };

BOOST_STATIC_ASSERT((sizeof(AlgoNames)/sizeof(*AlgoNames)) == MaxMerge);

template<class T>
bool measure_algo(T *elements, std::size_t key_reps[], std::size_t element_count, std::size_t key_len, unsigned alg, nanosecond_type &prev_clock)
{
   std::size_t const split_pos = generate_elements(elements, element_count, key_reps, key_len, counted_less<T>());

   std::printf("%s ", AlgoNames[alg]);
   num_compare=0;
   num_copy=0;
   num_elements = element_count;
   cpu_timer timer;
   timer.resume();
   switch(alg)
   {
      case InplaceMerge:
         std::inplace_merge(elements, elements+split_pos, elements+element_count, counted_less<T>());
      break;
      case AdaptiveMerge:
         boost::movelib::adaptive_merge(elements, elements+split_pos, elements+element_count, counted_less<T>());
      break;
      case SqrtHAdaptiveMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, counted_less<T>()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count)/2+1);
      break;
      case SqrtAdaptiveMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, counted_less<T>()
                            , boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case Sqrt2AdaptiveMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, counted_less<T>()
                            , 2*boost::movelib::detail_adaptive::ceil_sqrt_multiple(element_count));
      break;
      case QuartAdaptiveMerge:
         adaptive_merge_buffered( elements, elements+split_pos, elements+element_count, counted_less<T>()
                            , (element_count-1)/4+1);
      break;
      case BuflessMerge:
         boost::movelib::merge_bufferless(elements, elements+split_pos, elements+element_count, counted_less<T>());
      break;
   }
   timer.stop();

   if(num_elements == element_count){
      std::printf(" Tmp Ok ");
   } else{
      std::printf(" Tmp KO ");
   }
   nanosecond_type new_clock = timer.elapsed().wall;

   //std::cout << "Cmp:" << num_compare << " Cpy:" << num_copy;   //for old compilers without ll size argument
   std::printf("Cmp:%8.04f Cpy:%9.04f", double(num_compare)/element_count, double(num_copy)/element_count );

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
   bool res = test_order(elements, element_count, true);
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
   res = res && measure_algo(A,Keys,L,NK,InplaceMerge, prev_clock);
   back_clock = prev_clock;/*
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,QuartAdaptiveMerge, prev_clock);*/
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,Sqrt2AdaptiveMerge, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,SqrtAdaptiveMerge, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,SqrtHAdaptiveMerge, prev_clock);
   //
   prev_clock = back_clock;
   res = res && measure_algo(A,Keys,L,NK,AdaptiveMerge, prev_clock);
   //
   //prev_clock = back_clock;
   //res = res && measure_algo(A,Keys,L,NK,BuflessMerge, prev_clock);
   //
   if(!res)
      throw int(0);
   return res;
}

struct less
{
   template<class T, class U>
   bool operator()(const T &t, const U &u)
   {  return t < u;  }
};

//Undef it to run the long test
#define BENCH_MERGE_SHORT

int main()
{
   try{
   measure_all<merged_type>(101,1);
   measure_all<merged_type>(101,7);
   measure_all<merged_type>(101,31);
   measure_all<merged_type>(101,0);

   //
   measure_all<merged_type>(1101,1);
   measure_all<merged_type>(1001,7);
   measure_all<merged_type>(1001,31);
   measure_all<merged_type>(1001,127);
   measure_all<merged_type>(1001,511);
   measure_all<merged_type>(1001,0);
   //
   #ifndef BENCH_MERGE_SHORT
   measure_all<merged_type>(10001,65);
   measure_all<merged_type>(10001,255);
   measure_all<merged_type>(10001,1023);
   measure_all<merged_type>(10001,4095);
   measure_all<merged_type>(10001,0);

   //
   measure_all<merged_type>(100001,511);
   measure_all<merged_type>(100001,2047);
   measure_all<merged_type>(100001,8191);
   measure_all<merged_type>(100001,32767);
   measure_all<merged_type>(100001,0);

   //
   #ifdef NDEBUG
   measure_all<merged_type>(1000001,1);
   measure_all<merged_type>(1000001,1024);
   measure_all<merged_type>(1000001,32768);
   measure_all<merged_type>(1000001,524287);
   measure_all<merged_type>(1000001,0);
   measure_all<merged_type>(1500001,0);
   //measure_all<merged_type>(10000001,0);
   //measure_all<merged_type>(15000001,0);
   //measure_all<merged_type>(100000001,0);
   #endif   //NDEBUG

   #endif   //#ifndef BENCH_MERGE_SHORT

   //measure_all<merged_type>(100000001,0);
   }
   catch(...)
   {
      return 1;
   }

   return 0;
}

