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

#ifndef BOOST_MOVE_TEST_ORDER_TYPE_HPP
#define BOOST_MOVE_TEST_ORDER_TYPE_HPP

#include <boost/config.hpp>
#include <boost/move/core.hpp>
#include <cstddef>
#include <cstdio>

struct order_perf_type
{
   public:
   std::size_t key;
   std::size_t val;

   order_perf_type()
   {
      ++num_elements;
   }

   order_perf_type(const order_perf_type& other)
      : key(other.key), val(other.val)
   {
      ++num_elements;
      ++num_copy;
   }

   order_perf_type & operator=(const order_perf_type& other)
   {
      ++num_copy;
      key = other.key;
      val = other.val;
      return *this;
   }

   ~order_perf_type ()
   {
      --num_elements;
   }

   static void reset_stats()
   {
      num_compare=0;
      num_copy=0;
   }

   friend bool operator< (const order_perf_type& left, const order_perf_type& right)
   {  ++num_compare; return left.key < right.key;  }

   static boost::ulong_long_type num_compare;
   static boost::ulong_long_type num_copy;
   static boost::ulong_long_type num_elements;
};

boost::ulong_long_type order_perf_type::num_compare = 0;
boost::ulong_long_type order_perf_type::num_copy = 0;
boost::ulong_long_type order_perf_type::num_elements = 0;


struct order_move_type
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(order_move_type)

   public:
   std::size_t key;
   std::size_t val;

   order_move_type()
      : key(0u), val(0u)
   {}

   order_move_type(BOOST_RV_REF(order_move_type) other)
      : key(other.key), val(other.val)
   {
      other.key = other.val = std::size_t(-1);
   }

   order_move_type & operator=(BOOST_RV_REF(order_move_type) other)
   {
      key = other.key;
      val = other.val;
      other.key = other.val = std::size_t(-2);
      return *this;
   }

   friend bool operator< (const order_move_type& left, const order_move_type& right)
   {  return left.key < right.key;  }

   ~order_move_type ()
   {
      key = val = std::size_t(-3);
   }
};

struct order_type_less
{
   template<class T>
   bool operator()(const T &a, T const &b) const
   {  return a < b;   }
};

template<class T>
inline bool is_order_type_ordered(T *elements, std::size_t element_count, bool stable = true)
{
   for(std::size_t i = 1; i < element_count; ++i){
      if(order_type_less()(elements[i], elements[i-1])){
         std::printf("\n Ord KO !!!!");
         return false;
      }
      if( stable && !(order_type_less()(elements[i-1], elements[i])) && (elements[i-1].val > elements[i].val) ){
         std::printf("\n Stb KO !!!! ");
         return false;
      }
   }
   return true;
}

namespace boost {
namespace movelib {
namespace detail_adaptive {



}}}

template<class T>
inline bool is_key(T *elements, std::size_t element_count)
{
   for(std::size_t i = 1; i < element_count; ++i){
      if(elements[i].key >= element_count){
         std::printf("\n Key.key KO !!!!");
         return false;
      }
      if(elements[i].val != std::size_t(-1)){
         std::printf("\n Key.val KO !!!!");
         return false;
      }
   }
   return true;
}

template<class T>
inline bool is_buffer(T *elements, std::size_t element_count)
{
   for(std::size_t i = 1; i < element_count; ++i){
      if(elements[i].key != std::size_t(-1)){
         std::printf("\n Buf.key KO !!!!");
         return false;
      }
      if(elements[i].val >= element_count){
         std::printf("\n Buf.val KO !!!!");
         return false;
      }
   }
   return true;
}


#endif   //BOOST_MOVE_TEST_ORDER_TYPE_HPP
