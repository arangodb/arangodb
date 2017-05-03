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
#include <cstddef>
#include <cstdio>

struct order_type
{
   public:
   std::size_t key;
   std::size_t val;

   order_type()
   {
      ++num_elements;
   }

   order_type(const order_type& other)
      : key(other.key), val(other.val)
   {
      ++num_elements;
      ++num_copy;
   }

   order_type & operator=(const order_type& other)
   {
      ++num_copy;
      key = other.key;
      val = other.val;
      return *this;
   }

   ~order_type ()
   {
      --num_elements;
   }

   static boost::ulong_long_type num_compare;
   static boost::ulong_long_type num_copy;
   static boost::ulong_long_type num_elements;
};

boost::ulong_long_type order_type::num_compare = 0;
boost::ulong_long_type order_type::num_copy = 0;
boost::ulong_long_type order_type::num_elements = 0;

template<class T>
struct order_type_less
{
   bool operator()(const T &a,T const &b) const
   {  ++order_type::num_compare; return a.key < b.key;   }
};

template<class T>
inline bool is_order_type_ordered(T *elements, std::size_t element_count, bool stable = true)
{
   for(std::size_t i = 1; i < element_count; ++i){
      if(order_type_less<T>()(elements[i], elements[i-1])){
         std::printf("\n Ord KO !!!!");
         return false;
      }
      if( stable && !(order_type_less<T>()(elements[i-1], elements[i])) && (elements[i-1].val > elements[i].val) ){
         std::printf("\n Stb KO !!!! ");
         return false;
      }
   }
   return true;
}

#endif   //BOOST_MOVE_TEST_ORDER_TYPE_HPP
