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
#include <boost/move/detail/iterator_traits.hpp>
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

   static const std::size_t moved_constr_mark = std::size_t(-1);
   static const std::size_t moved_assign_mark = std::size_t(-2);

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
   template<class T, class U>
   bool operator()(const T &a, U const &b) const
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


//size_type iterator
template <class T, class D>
class randit
{
   public:
   typedef std::random_access_iterator_tag   iterator_category;
   typedef T                                 value_type;
   typedef D                                 difference_type;
   typedef T*                                pointer;
   typedef T&                                reference;

   private:
   T* m_ptr;

   public:
   explicit randit(T* ptr)
      : m_ptr(ptr)
   {}

   public:

   //Constructors
   randit()
      : m_ptr()   //Value initialization to achieve "null iterators" (N3644)
   {}

   randit(const randit& other)
      :  m_ptr(other.m_ptr)
   {}

   randit & operator=(const randit& other)
   {  m_ptr = other.m_ptr;   return *this;  }

   //T* like operators
   reference operator*()   const
   {  return *m_ptr;  }

   pointer operator->()  const
   {  return m_ptr;  }

   reference operator[](difference_type off) const
   {  return m_ptr[off];  }

   //Increment / Decrement
   randit& operator++()
   {  ++m_ptr;  return *this; }

   randit operator++(int)
   {  return randit(m_ptr++); }

   randit& operator--()
   {  --m_ptr; return *this;  }

   randit operator--(int)
   {  return randit(m_ptr--); }

   //Arithmetic
   randit& operator+=(difference_type off)
   {  m_ptr += off; return *this;   }

   randit& operator-=(difference_type off)
   {  m_ptr -= off; return *this;   }

   friend randit operator+(const randit &x, difference_type off)
   {  return randit(x.m_ptr+off);  }

   friend randit operator+(difference_type off, randit right)
   {  right.m_ptr += off;  return right; }

   friend randit operator-(randit left, difference_type off)
   {  left.m_ptr -= off;  return left; }

   friend difference_type operator-(const randit &left, const randit& right)
   {  return difference_type(left.m_ptr - right.m_ptr);   }

   //Comparison operators
   friend bool operator==   (const randit& l, const randit& r)
   {  return l.m_ptr == r.m_ptr;  }

   friend bool operator!=   (const randit& l, const randit& r)
   {  return l.m_ptr != r.m_ptr;  }

   friend bool operator<    (const randit& l, const randit& r)
   {  return l.m_ptr < r.m_ptr;  }

   friend bool operator<=   (const randit& l, const randit& r)
   {  return l.m_ptr <= r.m_ptr;  }

   friend bool operator>    (const randit& l, const randit& r)
   {  return l.m_ptr > r.m_ptr;  }

   friend bool operator>=   (const randit& l, const randit& r)
   {  return l.m_ptr >= r.m_ptr;  }
};

struct less_int
{
   bool operator()(int l, int r)
   {  return l < r;  }
};


#endif   //BOOST_MOVE_TEST_ORDER_TYPE_HPP
