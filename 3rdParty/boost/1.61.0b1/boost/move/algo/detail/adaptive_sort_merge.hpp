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
//
// Stable sorting that works in O(N*log(N)) worst time
// and uses O(1) extra memory
//
//////////////////////////////////////////////////////////////////////////////
//
// The main idea of the adaptive_sort algorithm was developed by Andrey Astrelin
// and explained in the article from the russian collaborative blog
// Habrahabr (http://habrahabr.ru/post/205290/). The algorithm is based on
// ideas from B-C. Huang and M. A. Langston explained in their article
// "Fast Stable Merging and Sorting in Constant Extra Space (1989-1992)"
// (http://comjnl.oxfordjournals.org/content/35/6/643.full.pdf).
//
// This implementation by Ion Gaztanaga uses previous ideas with additional changes:
// 
// - Use of GCD-based rotation.
// - Non power of two buffer-sizes.
// - Tries to find sqrt(len)*2 unique keys, so that the merge sort
//   phase can form up to sqrt(len)*4 segments if enough keys are found.
// - The merge-sort phase can take advantage of external memory to
//   save some additional combination steps.
// - Optimized comparisons when selection-sorting blocks as A and B blocks
//   are already sorted.
// - The combination phase is performed alternating merge to left and merge
//   to right phases minimizing swaps due to internal buffer repositioning.
// - When merging blocks special optimizations are made to avoid moving some
//   elements twice.
//
// The adaptive_merge algorithm was developed by Ion Gaztanaga reusing some parts
// from the sorting algorithm and implementing a block merge algorithm
// without moving elements left or right, which is used when external memory
// is available.
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_MOVE_ADAPTIVE_SORT_MERGE_HPP
#define BOOST_MOVE_ADAPTIVE_SORT_MERGE_HPP

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/algo/move.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/algo/detail/insertion_sort.hpp>
#include <boost/move/algo/detail/merge_sort.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/assert.hpp>
#include <boost/cstdint.hpp>

#ifdef BOOST_MOVE_ADAPTIVE_SORT_STATS
   #define BOOST_MOVE_ADAPTIVE_SORT_PRINT(STR, L) \
      print_stats(STR, L)\
   //
#else
   #define BOOST_MOVE_ADAPTIVE_SORT_PRINT(STR, L)
#endif

namespace boost {
namespace movelib {

namespace detail_adaptive {

static const std::size_t AdaptiveSortInsertionSortThreshold = 16;
//static const std::size_t AdaptiveSortInsertionSortThreshold = 4;
BOOST_STATIC_ASSERT((AdaptiveSortInsertionSortThreshold&(AdaptiveSortInsertionSortThreshold-1)) == 0);

#if defined BOOST_HAS_INTPTR_T
   typedef ::boost::uintptr_t uintptr_t;
#else
   typedef std::size_t uintptr_t;
#endif

template<class T>
const T &min_value(const T &a, const T &b)
{
   return a < b ? a : b;
}

template<class T>
const T &max_value(const T &a, const T &b)
{
   return a > b ? a : b;
}

template<class T>
class adaptive_xbuf
{
   adaptive_xbuf(const adaptive_xbuf &);
   adaptive_xbuf & operator=(const adaptive_xbuf &);

   public:
   typedef T* iterator;

   adaptive_xbuf()
      : m_ptr(0), m_size(0), m_capacity(0)
   {}

   adaptive_xbuf(T *raw_memory, std::size_t capacity)
      : m_ptr(raw_memory), m_size(0), m_capacity(capacity)
   {}

   template<class RandIt>
   void move_assign(RandIt first, std::size_t n)
   {
      if(n <= m_size){
         boost::move(first, first+n, m_ptr);
         std::size_t size = m_size;
         while(size-- != n){
            m_ptr[size].~T();
         }
         m_size = n;
      }
      else{
         T *result = boost::move(first, first+m_size, m_ptr);
         boost::uninitialized_move(first+m_size, first+n, result);
         m_size = n;
      }
   }

   template<class RandIt>
   void push_back(RandIt first, std::size_t n)
   {
      BOOST_ASSERT(m_capacity - m_size >= n);
      boost::uninitialized_move(first, first+n, m_ptr+m_size);
      m_size += n;
   }

   template<class RandIt>
   iterator add(RandIt it)
   {
      BOOST_ASSERT(m_size < m_capacity);
      T * p_ret = m_ptr + m_size;
      ::new(p_ret) T(::boost::move(*it));
      ++m_size;
      return p_ret;
   }

   template<class RandIt>
   void insert(iterator pos, RandIt it)
   {
      if(pos == (m_ptr + m_size)){
         this->add(it);
      }
      else{
         this->add(m_ptr+m_size-1);
         //m_size updated
         boost::move_backward(pos, m_ptr+m_size-2, m_ptr+m_size-1);
         *pos = boost::move(*it);
      }
   }

   void set_size(std::size_t size)
   {
      m_size = size;
   }

   template<class U>
   bool supports_aligned_trailing(std::size_t size, std::size_t trail_count) const
   {
      if(this->data()){
         uintptr_t u_addr_sz = uintptr_t(this->data()+size);
         uintptr_t u_addr_cp = uintptr_t(this->data()+this->capacity());
         u_addr_sz = ((u_addr_sz + sizeof(U)-1)/sizeof(U))*sizeof(U);
         
         return (u_addr_cp >= u_addr_sz) && ((u_addr_cp - u_addr_sz)/sizeof(U) >= trail_count);
      }
      return false;
   }

   template<class U>
   U *aligned_trailing() const
   {
      return this->aligned_trailing<U>(this->size());
   }

   template<class U>
   U *aligned_trailing(std::size_t pos) const
   {
      uintptr_t u_addr = uintptr_t(this->data()+pos);
      u_addr = ((u_addr + sizeof(U)-1)/sizeof(U))*sizeof(U);
      return (U*)u_addr;
   }

   ~adaptive_xbuf()
   {
      this->clear();
   }

   std::size_t capacity() const
   {  return m_capacity;   }

   iterator data() const
   {  return m_ptr;   }

   iterator end() const
   {  return m_ptr+m_size;   }

   std::size_t size() const
   {  return m_size;   }

   bool empty() const
   {  return !m_size;   }

   void clear()
   {
      std::size_t size = m_size;
      while(size--){
         m_ptr[size].~T();
      }
      m_size = 0u;
   }

   private:
   T *m_ptr;
   std::size_t m_size;
   std::size_t m_capacity;
};

template<class Iterator, class Op>
class range_xbuf
{
   range_xbuf(const range_xbuf &);
   range_xbuf & operator=(const range_xbuf &);

   public:
   typedef typename iterator_traits<Iterator>::size_type size_type;
   typedef Iterator iterator;

   range_xbuf(Iterator first, Iterator last)
      : m_first(first), m_last(first), m_cap(last)
   {}

   template<class RandIt>
   void move_assign(RandIt first, std::size_t n)
   {
      BOOST_ASSERT(size_type(n) <= size_type(m_cap-m_first));
      m_last = Op()(forward_t(), first, first+n, m_first);
   }

   ~range_xbuf()
   {}

   std::size_t capacity() const
   {  return m_cap-m_first;   }

   Iterator data() const
   {  return m_first;   }

   Iterator end() const
   {  return m_last;   }

   std::size_t size() const
   {  return m_last-m_first;   }

   bool empty() const
   {  return m_first == m_last;   }

   void clear()
   {
      m_last = m_first;
   }

   template<class RandIt>
   iterator add(RandIt it)
   {
      Iterator pos(m_last);
      *pos = boost::move(*it);
      ++m_last;
      return pos;
   }

   void set_size(std::size_t size)
   {
      m_last  = m_first;
      m_last += size;
   }

   private:
   Iterator const m_first;
   Iterator m_last;
   Iterator const m_cap;
};

template<class RandIt, class Buf>
bool three_way_init( RandIt first1, RandIt last1, Buf &buf
                   , typename Buf::iterator &buf_first, typename Buf::iterator &buf_last, move_op)
{
   buf.move_assign(first1, last1-first1);
   buf_first = buf.data();
   buf_last  = buf.end();
   return true;
}

template<class RandIt, class Buf>
bool three_way_init( RandIt first, RandIt last, Buf &buf
                   , typename Buf::iterator &buf_first, typename Buf::iterator &buf_last, swap_op)
{
   typedef typename iterator_traits<RandIt>::size_type   size_type;
   buf.clear();
   buf_first = buf.data();
   buf_last  = buf_first + size_type(last-first);
   return false;
}

template<class T, class Buf>
void three_way_move(T &a, T &b, Buf &buf, move_op)
{
   buf.add(&b);
   b = boost::move(a);
}

template<class T, class Buf>
void three_way_move(T &a, T &b, Buf &buf, swap_op)
{
   T tmp(boost::move(*buf.end()));
   buf.add(&b);
   b = boost::move(a);
   a = boost::move(tmp);
}

///////////////////////////////////////////////////////////////////////////////
//
//                         PARTIAL MERGE BUF
//
///////////////////////////////////////////////////////////////////////////////


template<class Buf, class RandIt, class Compare, class Op>
RandIt op_partial_merge_with_buf_impl
   ( RandIt first1, RandIt const last1, RandIt first2, RandIt last2
   , Buf &buf, typename Buf::iterator &buf_first1_in_out, typename Buf::iterator &buf_last1_in_out
   , Compare comp, Op op
   )
{
   typedef typename Buf::iterator buf_iterator;
   typedef typename iterator_traits<RandIt>::value_type  value_type;

   BOOST_ASSERT(first1 != last1);
   BOOST_ASSERT(first2 != last2);
   buf_iterator buf_first1 = buf_first1_in_out;
   buf_iterator buf_last1  = buf_last1_in_out;

   if(buf_first1 == buf_last1){
      //Skip any element that does not need to be moved
      while(!comp(*last1, *first1)){
         ++first1;
         if(first1 == last1){
            return first1;
         }
      }

      //If initialization is successful, move to buffer while merging
      //Three way moves need less moves when op is swap_op so use it
      //when merging elements from range2 to the destination occupied by range1
      if(!three_way_init(first1, last1, buf, buf_first1, buf_last1, op)){
         three_way_move(*first2, *first1, buf, op);
         for(++first1, ++first2; first1 != last1; ++first1){
            value_type &v = comp(*first2, *buf_first1) ? *first2++ : *buf_first1++;
            three_way_move(v, *first1, buf, op);
         }
      }
   }

   //Now merge from buffer
   if(first2 != last2)
   while(1){
		if(comp(*first2, *buf_first1)) {
         op(first2++, first1++);
         if(first2 == last2)
            break;
      }
      else{
         op(buf_first1++, first1++);
         if(buf_first1 == buf_last1)
            break;
      }
   }

   buf_first1_in_out = buf_first1;
   buf_last1_in_out  = buf_last1;
   return first1;
}

template<class RandIt, class Buf, class Compare, class Op>
RandIt op_partial_merge_with_buf
   ( RandIt first1, RandIt const last1, RandIt first2, RandIt last2
   , Buf &buf
   , typename Buf::iterator &buf_first1_in_out
   , typename Buf::iterator &buf_last1_in_out
   , Compare comp
   , Op op
   , bool is_stable)
{
   return is_stable
      ? op_partial_merge_with_buf_impl
         (first1, last1, first2, last2, buf, buf_first1_in_out, buf_last1_in_out, comp, op)
      : op_partial_merge_with_buf_impl
         (first1, last1, first2, last2, buf, buf_first1_in_out, buf_last1_in_out, antistable<Compare>(comp), op)
      ;
}

// key_first - sequence of keys, in same order as blocks. key_comp(key, midkey) means stream A
// first - first element to merge.
// first[-l_block, 0) - buffer
// l_block - length of regular blocks. Blocks are stable sorted by 1st elements and key-coded
// l_irreg1 is the irregular block to be merged before n_bef_irreg2 blocks (can be 0)
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be merged after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare, class Op, class Buf>
void op_merge_blocks_with_buf
   ( RandItKeys key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , Op op
   , Buf & xbuf)
{
   if(n_bef_irreg2 == 0){
      RandIt const last_reg(first+l_irreg1+n_aft_irreg2*l_block);
      op_buffered_merge(first, last_reg, last_reg+l_irreg2, comp, op, xbuf);
   }
   else {
      typedef typename Buf::iterator buf_iterator;
      buf_iterator buffer = xbuf.data();
      buf_iterator buffer_end = buffer;
      RandIt first1 = first;
      RandIt last1  = l_irreg1 ? first1 + l_irreg1 : first1 + l_block;
      RandIt first2 = last1;
      RandItKeys const key_end (key_first+n_bef_irreg2);

      for( bool is_range1_A = l_irreg1 ? true : key_comp(*key_first, midkey), skip_first_it = l_irreg1 != 0
         ; key_first != key_end; ){
         if(!skip_first_it){
            ++key_first;
         }
         skip_first_it = false;
         bool const last_it = key_first == key_end;
         //If the trailing block is empty, we'll make it equal to the previous if empty
		   bool const is_range2_A  = last_it ? (!l_irreg2 && is_range1_A) : key_comp(*key_first, midkey);

         if(is_range1_A == is_range2_A){
            if(buffer != buffer_end){
               first1 = op(forward_t(), buffer, buffer_end, first1);
               BOOST_ASSERT(first1 == first2);
               buffer_end = buffer;
            }
            first1 = first2;
            if(last_it){
               xbuf.clear();
               last1 = first2+l_block*n_aft_irreg2;
               op_buffered_merge(first1, last1, last1+l_irreg2, comp, op, xbuf);
               break;
            }
            else{
               last1 = first2 + l_block;
            }
            first2 += l_block;
         }
         else {
            BOOST_ASSERT(!last_it || (l_irreg2 || n_aft_irreg2));
            if(last_it){
               RandIt res = op(forward_t(), buffer, buffer_end, first1);
               BOOST_ASSERT(buffer == buffer_end || res == last1); (void)res;
               last1 += l_block*n_aft_irreg2;
               xbuf.clear();
               op_buffered_merge(first1, last1, last1+l_irreg2, comp, op, xbuf);
               break;
            }
            else{
               RandIt const last2  = first2 + l_block;
               first1 = op_partial_merge_with_buf(first1, last1, first2, last2, xbuf, buffer, buffer_end, comp, op, is_range1_A);
               if(buffer == buffer_end){
                  is_range1_A = is_range2_A;
               }
               last1  = last2;
               first2 = last1;
               BOOST_ASSERT((buffer == buffer_end) || (buffer_end-buffer) == (last1-first1));
            }
         }
      }
   }
}

template<class RandItKeys, class KeyCompare, class RandIt, class Compare, class Buf>
void merge_blocks_with_buf
   ( RandItKeys key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , Buf & xbuf
   , bool const xbuf_used)
{
   if(xbuf_used){
      op_merge_blocks_with_buf
         (key_first, midkey, key_comp, first, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, move_op(), xbuf);
   }
   else{
      op_merge_blocks_with_buf
         (key_first, midkey, key_comp, first, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, swap_op(), xbuf);
   }
}

///////////////////////////////////////////////////////////////////////////////
//
//                         PARTIAL MERGE LEFT
//
///////////////////////////////////////////////////////////////////////////////

template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_left_middle_buffer_impl
   (RandIt first1, RandIt const last1, RandIt const first2
   , const typename iterator_traits<RandIt>::value_type &next_key, Compare comp
   , Op op)
{
   while(first1 != last1 && !comp(next_key, *first1)){
      ++first1;
   }
   //Even if we copy backward, no overlapping occurs so use forward copy
   //that can be faster specially with trivial types
   RandIt const new_first1 = first2 - (last1 - first1);
   BOOST_ASSERT(last1 <= new_first1);
   op(forward_t(), first1, last1, new_first1);
   return new_first1;
}

template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_left_middle_buffer
   ( RandIt first1, RandIt const last1, RandIt const first2
   , const typename iterator_traits<RandIt>::value_type &next_key, Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_left_middle_buffer_impl(first1, last1, first2, next_key, comp, op)
                    : op_partial_merge_left_middle_buffer_impl(first1, last1, first2, next_key, antistable<Compare>(comp), op);
}

// Partially merges two ordered ranges. Partially means that elements are merged
// until one of two ranges is exhausted (M elements from ranges 1 y 2).
// [buf_first, ...) -> buffer that can be overwritten
// [first1, last1) merge [last1,last2) -> [buf_first, buf_first+M)
// Note: distance(buf_first, first1) >= distance(last1, last2), so no overlapping occurs.
template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_left_impl
   ( RandIt buf_first, RandIt first1, RandIt const last1, RandIt const last2, Compare comp, Op op)
{
   RandIt first2 = last1;
   while(first1 != last1){
      if(first2 == last2){
         return first1;
	   }
		if(comp(*first2, *first1)) {
         op(first2, buf_first);
         ++first2;
      }
      else{
         op(first1, buf_first);
         ++first1;
      }
      ++buf_first;
   }
   return first2;
}


template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_left
   ( RandIt buf_first, RandIt first1, RandIt const last1, RandIt const last2, Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_left_impl(buf_first, first1, last1, last2, comp, op)
                    : op_partial_merge_left_impl(buf_first, first1, last1, last2, antistable<Compare>(comp), op);
}

template<class RandIt>
bool three_way_side_init( RandIt first1, RandIt last1, RandIt buf_first1, move_op)
{
   boost::move(first1, last1, buf_first1);
   return true;
}

template<class RandIt>
bool three_way_side_init( RandIt, RandIt, RandIt, swap_op)
{
   return false;
}

template<class T>
void three_way_side_move(T &a, T &b, T&c, move_op)
{
   c = boost::move(b);
   b = boost::move(a);
}

template<class T>
void three_way_side_move(T &a, T &b, T &c, swap_op)
{
   T tmp(boost::move(c));
   c = boost::move(b);
   b = boost::move(a);
   a = boost::move(tmp);
}


// Partially merges two ordered ranges. Partially means that elements are merged
// until one of two ranges is exhausted (M elements from ranges 1 y 2).
// [buf_first, ...) -> buffer that can be overwritten
// [first1, last1) merge [last1,last2) -> [buf_first, buf_first+M)
// Note: distance(buf_first, first1) >= distance(last1, last2), so no overlapping occurs.
template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_left_smart_impl
   ( RandIt first1, RandIt last1, RandIt first2, RandIt const last2, Compare comp, Op op)
{
   typedef typename iterator_traits<RandIt>::value_type  value_type;

   RandIt dest;
   if(last1 != first2){
      BOOST_ASSERT((first2-last1)==(last2-first2));
      //Skip any element that does not need to be moved
      while(!comp(*first2, *first1)){
         ++first1;
         if(first1 == last1)
            return first2;
      }

      RandIt buf_first1 = first2 - (last1-first1);

      //If initialization is successful, move to buffer while merging
      //Three way moves need less moves when op is swap_op so use it
      //when merging elements from range2 to the destination occupied by range1
      if(!three_way_side_init(first1, last1, buf_first1, op)){
         RandIt buf_last1 = buf_first1;
         three_way_side_move(*first2, *first1, *buf_last1++, op);

         RandIt const orig_first2 = first2;(void)(orig_first2);
         for(++first1, ++first2; first1 != last1; ++first1, ++buf_last1){
            value_type &v = comp(*first2, *buf_first1) ? *first2++ : *buf_first1++;
            three_way_side_move(v, *first1, *buf_last1, op);
         }
         BOOST_ASSERT(buf_last1 == orig_first2);
         last1 = buf_last1;
      }
      else{
         last1 = first2;
      }
      dest   = first1;
      first1 = buf_first1;
      BOOST_ASSERT((first1-dest) == (last2-first2));
   }
   else{
      dest = first1-(last2-first2);
   }

   BOOST_ASSERT(0 != (last1-first1));
   if(first2 != last2)
   while(1){
		if(comp(*first2, *first1)) {
         op(first2++, dest++);
         if(first2 == last2){
            return first1;
   	   }
      }
      else{
         op(first1++, dest++);
         if(first1 == last1){
            return first2;
         }
      }
   }
   return first1;
}



template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_left_smart
   (RandIt first1, RandIt const last1, RandIt first2, RandIt const last2, Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_left_smart_impl(first1, last1, first2, last2, comp, op)
                    : op_partial_merge_left_smart_impl(first1, last1, first2, last2, antistable<Compare>(comp), op);
}


// first - first element to merge.
// first[-l_block, 0) - buffer
// l_block - length of regular blocks. Blocks are stable sorted by 1st elements and key-coded
// key_first - sequence of keys, in same order as blocks. key<midkey means stream A
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be merged after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare, class Op>
void op_merge_blocks_left
   ( RandItKeys key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp, Op op)
{
   if(n_bef_irreg2 == 0){
      RandIt const last_reg(first+l_irreg1+n_aft_irreg2*l_block);
		op_merge_left(first-l_block, first, last_reg, last_reg+l_irreg2, comp, op);
   }
   else {
      RandIt buffer = first  - l_block;
      RandIt first1 = first;
      RandIt last1  = l_irreg1 ? first1 + l_irreg1 : first1 + l_block;
      RandIt first2 = last1;
      RandItKeys const key_end (key_first+n_bef_irreg2);
      bool skip_first_it = l_irreg1 != 0;
      for( bool is_range1_A = l_irreg1 ? true : key_comp(*key_first, midkey)
         ; key_first != key_end; first2 += l_block){
         if(!skip_first_it){
            ++key_first;
         }
         skip_first_it = false;
         bool const last_it = key_first == key_end;
         //If the trailing block is empty, we'll make it equal to the previous if empty
		   bool const is_range2_A    = last_it ? (!l_irreg2 && is_range1_A) : key_comp(*key_first, midkey);
		   bool const is_buffer_middle = last1 == buffer;

         if(is_range1_A == is_range2_A){
            //If range1 is buffered, write it to its final position
            if(!is_buffer_middle){
               buffer = op(forward_t(), first1, last1, buffer);
            }

            first1 = first2;
            if(last_it){
               last1  = first2+l_block*n_aft_irreg2;
               op_merge_left(buffer, first1, last1, last1+l_irreg2, comp, op);
               break;
            }
            else{
               last1 = first2 + l_block;
            }
         }
         else {
            BOOST_ASSERT(!last_it || (l_irreg2 || n_aft_irreg2));
            if(last_it){
               if(is_buffer_middle){
                  //See comment below marked with (*)
                  first1 = op_partial_merge_left_middle_buffer(first1, last1, first2, first2[l_block*n_aft_irreg2], comp, op, is_range1_A);
                  last1  = first2;
                  buffer = first1 - l_block;
               }
               last1 += l_block*n_aft_irreg2;
               op_merge_left(buffer, first1, last1, last1+l_irreg2, comp, op);
               break;
            }
            else{
               RandIt const last2  = first2 + l_block;
               first1 = op_partial_merge_left_smart(first1, last1, first2, last2, comp, op, is_range1_A);

               if(first1 < first2){  //is_buffer_middle == true for the next iteration
                  last1  = first2;
                  buffer = last1;
               }
               else{ //is_buffer_middle == false for the next iteration
                  is_range1_A = is_range2_A;
                  buffer = first1 - l_block;
                  last1 = last2;
               }
            }
         }
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
//
//                         PARTIAL MERGE RIGHT
//
///////////////////////////////////////////////////////////////////////////////


template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_right_middle_buffer_impl
   ( RandIt const last1, RandIt const first2, RandIt last2, Compare comp, Op op)
{
   while(first2 != last2 && !comp(last2[-1], last1[-1])){
      --last2;
   }
   return op(forward_t(), first2, last2, last1);
}

template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_right_middle_buffer
   ( RandIt const last1, RandIt first2, RandIt const last2
   , Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_right_middle_buffer_impl(last1, first2, last2, comp, op)
                    : op_partial_merge_right_middle_buffer_impl(last1, first2, last2, antistable<Compare>(comp), op);
}

// Partially merges two ordered ranges. Partially means that elements are merged
// until one of two ranges is exhausted (M elements from ranges 1 y 2).
// [last2, buf_last) -> buffer that can be overwritten
// [first1, last1) merge [last1,last2) -> [buf_last - M, buf_last)
// Note: distance(last2, buf_last) >= distance(first1, last1), so no overlapping occurs.
template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_right_impl
   ( RandIt const first1, RandIt last1, RandIt last2, RandIt buf_last, Compare comp, Op op)
{
   RandIt const first2 = last1;
   while(first2 != last2){
		if(last1 == first1){
         return last2;
      }
      --last2;
      --last1;
      --buf_last;
      if(comp(*last2, *last1)){
         op(last1, buf_last);
         ++last2;
      }
      else{
         op(last2, buf_last);
         ++last1;
      }
   }
   return last1;
}

template<class RandIt, class Compare, class Op>
RandIt op_partial_merge_right
   ( RandIt first1, RandIt const last1, RandIt const last2, RandIt buf_last, Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_right_impl(first1, last1, last2, buf_last, comp, op)
                    : op_partial_merge_right_impl(first1, last1, last2, buf_last, antistable<Compare>(comp), op);
}


// first - first element to merge.
// last iterator is (first+l_block*(n_bef_irreg2+n_aft_irreg2)+l_irreg2)
// [last, last+l_block) - buffer
// l_block - length of regular blocks. Blocks are stable sorted by 1st elements and key-coded
// key_first - sequence of keys, in same order as blocks. key<midkey means stream A
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be merged after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare, class Op>
void op_merge_blocks_right
   ( RandItKeys const key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp, Op op)
{
   RandIt last1  = first + (n_bef_irreg2+n_aft_irreg2)*l_block;
   RandIt first1 = last1-l_block;
   RandIt first2 = last1;
   RandIt last2  = first2 + l_irreg2;
   RandIt buffer_end = last2 + l_block;
   RandItKeys key_end (key_first+(n_bef_irreg2+n_aft_irreg2));

   for(bool is_range2_A = false; key_first != key_end; last1 = first1, first1 -= l_block){
      --key_end;
      bool const is_range1_A = key_comp(*key_end, midkey);
		bool const is_buffer_middle = first2 == buffer_end;

      if(is_range1_A == is_range2_A){
         if(!is_buffer_middle){
            buffer_end = op(backward_t(), first2, last2, buffer_end);
         }
         //else //op forward already done on previous op_partial_merge_right

         first2 = first1;
         last2  = last1;
      }
      else {
         if(is_buffer_middle){
            //A previous op_partial_merge_right has right range2 elements after the buffer.
            //In order to merge it with the next block, move them to the start of the buffer so that
            //buffer is placed to the right. Move only the minimum elements as some range1 elements
            //won't be moved in the merge.
            last2  = op_partial_merge_right_middle_buffer(last1, first2, last2, comp, op, !is_range2_A);
            first2 = last1;
            buffer_end = last2 + l_block;
         }

         //op_partial_merge_right merges two ranges, but stops moving elements 
         //once one range is emptied to avoid moving data twice in the next iteration
         last2 = op_partial_merge_right(first1, last1, last2, buffer_end, comp, op, !is_range2_A);
         if(last2 > first2){  //is_buffer_middle == true for the next iteration
            buffer_end = first2;
         }
         else{ //is_buffer_middle == false for the next iteration
            is_range2_A = is_range1_A;
            buffer_end = last2 + l_block;
            first2 = first1;
         }
      }
   }

   if(first2 != buffer_end){
      op(backward_t(), first2, last2, buffer_end);
   }
}

///////////////////////////////////////////////////////////////////////////////
//
//                         PARTIAL MERGE BUFFERLESS
//
///////////////////////////////////////////////////////////////////////////////

// [first1, last1) merge [last1,last2) -> [first1,last2)
template<class RandIt, class Compare>
RandIt partial_merge_bufferless_impl
   (RandIt first1, RandIt last1, RandIt const last2, bool *const pis_range1_A, Compare comp)
{
   if(last1 == last2){
      return first1;
   }
   bool const is_range1_A = *pis_range1_A;
	if(first1 != last1 && comp(*last1, last1[-1])){
      do{
         RandIt const old_last1 = last1;
			last1  = lower_bound(last1, last2, *first1, comp);
         first1 = rotate_gcd(first1, old_last1, last1);//old_last1 == last1 supported
         if(last1 == last2){
            return first1;
         }
         do{
            ++first1;
			} while(last1 != first1 && !comp(*last1, *first1) );
      } while(first1 != last1);
   }
   *pis_range1_A = !is_range1_A;
   return last1;
}

// [first1, last1) merge [last1,last2) -> [first1,last2)
template<class RandIt, class Compare>
RandIt partial_merge_bufferless
   (RandIt first1, RandIt last1, RandIt const last2, bool *const pis_range1_A, Compare comp)
{
   return *pis_range1_A ? partial_merge_bufferless_impl(first1, last1, last2, pis_range1_A, comp)
                        : partial_merge_bufferless_impl(first1, last1, last2, pis_range1_A, antistable<Compare>(comp));
}



// l_block - length of regular blocks. First nblocks are stable sorted by 1st elements and key-coded
// keys - sequence of keys, in same order as blocks. key<midkey means stream A
// n_aft_irreg2 are regular blocks from stream A. l_irreg2 is length of last (irregular) block from stream B, that should go before n_aft_irreg2 blocks.
// l_irreg2=0 requires n_aft_irreg2=0 (no irregular blocks). l_irreg2>0, n_aft_irreg2=0 is possible.
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void merge_blocks_bufferless
   ( RandItKeys key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp)
{
   if(n_bef_irreg2 == 0){
      RandIt const last_reg(first+l_irreg1+n_aft_irreg2*l_block);
      merge_bufferless(first, last_reg, last_reg+l_irreg2, comp);
   }
   else{
      RandIt first1 = first;
      RandIt last1  = l_irreg1 ? first + l_irreg1: first + l_block;
      RandItKeys const key_end (key_first+n_bef_irreg2);
      bool is_range1_A = l_irreg1 ? true : key_comp(*key_first++, midkey);

      for( ; key_first != key_end; ++key_first){
		   bool is_range2_A = key_comp(*key_first, midkey);
         if(is_range1_A == is_range2_A){
            first1 = last1;
         }
         else{
            first1 = partial_merge_bufferless(first1, last1, last1 + l_block, &is_range1_A, comp);
         }
         last1 += l_block;
      }

      if(l_irreg2){
         if(!is_range1_A){
            first1 = last1;
         }
         last1 += l_block*n_aft_irreg2;
         merge_bufferless(first1, last1, last1+l_irreg2, comp);
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
//
//                            BUFFERED MERGE
//
///////////////////////////////////////////////////////////////////////////////
template<class RandIt, class Compare, class Op, class Buf>
void op_buffered_merge
      ( RandIt first, RandIt const middle, RandIt last
      , Compare comp, Op op
      , Buf &xbuf)
{
   if(first != middle && middle != last && comp(*middle, middle[-1])){
      typedef typename iterator_traits<RandIt>::size_type   size_type;
      size_type const len1 = size_type(middle-first);
      size_type const len2 = size_type(last-middle);
      if(len1 <= len2){
         first = upper_bound(first, middle, *middle, comp);
         xbuf.move_assign(first, size_type(middle-first));
         op_merge_with_right_placed
            (xbuf.data(), xbuf.end(), first, middle, last, comp, op);
      }
      else{
         last = lower_bound(middle, last, middle[-1], comp);
         xbuf.move_assign(middle, size_type(last-middle));
         op_merge_with_left_placed
            (first, middle, last, xbuf.data(), xbuf.end(), comp, op);
      }
   }
}

template<class RandIt, class Compare>
void buffered_merge
      ( RandIt first, RandIt const middle, RandIt last
      , Compare comp
      , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> &xbuf)
{
   op_buffered_merge(first, middle, last, comp, move_op(), xbuf);
}

// Complexity: 2*distance(first, last)+max_collected^2/2
//
// Tries to collect at most n_keys unique elements from [first, last),
// in the begining of the range, and ordered according to comp
// 
// Returns the number of collected keys
template<class RandIt, class Compare>
typename iterator_traits<RandIt>::size_type
   collect_unique
      ( RandIt const first, RandIt const last
      , typename iterator_traits<RandIt>::size_type const max_collected, Compare comp
      , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   typedef typename iterator_traits<RandIt>::value_type value_type;
   size_type h = 0;
   if(max_collected){
      ++h;  // first key is always here
      RandIt h0 = first;
      RandIt u = first; ++u;
      RandIt search_end = u;

      if(xbuf.capacity() >= max_collected){
         value_type *const ph0 = xbuf.add(first);
         while(u != last && h < max_collected){
		      value_type * const r = lower_bound(ph0, xbuf.end(), *u, comp);
            //If key not found add it to [h, h+h0)
		      if(r == xbuf.end() || comp(*u, *r) ){
               RandIt const new_h0 = boost::move(search_end, u, h0);
               search_end = u;
               ++search_end;
               ++h;
               xbuf.insert(r, u);
               h0 = new_h0;
            }
            ++u;
         }
         boost::move_backward(first, h0, h0+h);
         boost::move(xbuf.data(), xbuf.end(), first);
      }
      else{
         while(u != last && h < max_collected){
		      RandIt const r = lower_bound(h0, search_end, *u, comp);
            //If key not found add it to [h, h+h0)
		      if(r == search_end || comp(*u, *r) ){
               RandIt const new_h0 = rotate_gcd(h0, search_end, u);
               search_end = u;
               ++search_end;
               ++h;
               rotate_gcd(r+(new_h0-h0), u, search_end);
               h0 = new_h0;
            }
            ++u;
         }
         rotate_gcd(first, h0, h0+h);
      }
   }
   return h;
}

template<class Unsigned>
Unsigned floor_sqrt(Unsigned const n)
{
   Unsigned x = n;
   Unsigned y = x/2 + (x&1);
   while (y < x){
      x = y;
      y = (x + n / x)/2;
   }
   return x;
}

template<class Unsigned>
Unsigned ceil_sqrt(Unsigned const n)
{
   Unsigned r = floor_sqrt(n);
   return r + Unsigned((n%r) != 0);
}

template<class Unsigned>
Unsigned floor_merge_multiple(Unsigned const n, Unsigned &base, Unsigned &pow)
{
   Unsigned s = n;
   Unsigned p = 0;
   while(s > AdaptiveSortInsertionSortThreshold){
      s /= 2;
      ++p;
   }
   base = s;
   pow = p;
   return s << p;
}

template<class Unsigned>
Unsigned ceil_merge_multiple(Unsigned const n, Unsigned &base, Unsigned &pow)
{
   Unsigned fm = floor_merge_multiple(n, base, pow);

   if(fm != n){
      if(base < AdaptiveSortInsertionSortThreshold){
         ++base;
      }
      else{
         base = AdaptiveSortInsertionSortThreshold/2 + 1;
         ++pow;
      }
   }
   return base << pow;
}

template<class Unsigned>
Unsigned ceil_sqrt_multiple(Unsigned const n, Unsigned *pbase = 0)
{
   Unsigned const r = ceil_sqrt(n);
   Unsigned pow = 0;
   Unsigned base = 0;
   Unsigned const res = ceil_merge_multiple(r, base, pow);
   if(pbase) *pbase = base;
   return res;
}

template<class Unsigned>
Unsigned ceil_sqrt_pow2(Unsigned const n)
{
   Unsigned r=1;
   Unsigned exp = 0;
   Unsigned pow = 1u;
   while(pow != 0 && pow < n){
      r*=2;
      ++exp;
      pow = r << exp;
   }
   return r;
}

struct less
{
   template<class T>
   bool operator()(const T &l, const T &r)
   {  return l < r;  }
};

///////////////////////////////////////////////////////////////////////////////
//
//                            MERGE BLOCKS
//
///////////////////////////////////////////////////////////////////////////////

template<class RandIt, class Compare>
void slow_stable_sort
   ( RandIt const first, RandIt const last, Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type L = size_type(last - first);
   {  //Use insertion sort to merge first elements
      size_type m = 0;
      while((L - m) > size_type(AdaptiveSortInsertionSortThreshold)){
         insertion_sort(first+m, first+m+size_type(AdaptiveSortInsertionSortThreshold), comp);
         m += AdaptiveSortInsertionSortThreshold;
      }
      insertion_sort(first+m, last, comp);
   }

   size_type h = AdaptiveSortInsertionSortThreshold;
   for(bool do_merge = L > h; do_merge; h*=2){
      do_merge = (L - h) > h;
      size_type p0 = 0;
      if(do_merge){
         size_type const h_2 = 2*h;
         while((L-p0) > h_2){
			   merge_bufferless(first+p0, first+p0+h, first+p0+h_2, comp);
            p0 += h_2;
         }
      }
		if((L-p0) > h){
         merge_bufferless(first+p0, first+p0+h, last, comp);
      }
   }
}

//Returns new l_block and updates use_buf
template<class Unsigned>
Unsigned lblock_for_combine
   (Unsigned const l_block, Unsigned const n_keys, Unsigned const l_data, bool &use_buf)
{
   BOOST_ASSERT(l_data > 1);

   //We need to guarantee lblock >= l_merged/(n_keys/2) keys for the combination.
   //We have at least 4 keys guaranteed (which are the minimum to merge 2 ranges)
   //If l_block != 0, then n_keys is already enough to merge all blocks in all
   //phases as we've found all needed keys for that buffer and length before.
   //If l_block == 0 then see if half keys can be used as buffer and the rest
   //as keys guaranteeing that n_keys >= (2*l_merged)/lblock = 
   if(!l_block){
      //If l_block == 0 then n_keys is power of two
      //(guaranteed by build_params(...))
      BOOST_ASSERT(n_keys >= 4);
      //BOOST_ASSERT(0 == (n_keys &(n_keys-1)));

      //See if half keys are at least 4 and if half keys fulfill
      Unsigned const new_buf  = n_keys/2;
      Unsigned const new_keys = n_keys-new_buf;
      use_buf = new_keys >= 4 && new_keys >= l_data/new_buf;
      if(use_buf){
         return new_buf;
      }
      else{
         return l_data/n_keys;
      }
   }
   else{
      use_buf = true;
      return l_block;
   }
}


//Although "cycle" sort is known to have the minimum number of writes to target
//selection sort is more appropriate here as we want to minimize swaps.
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void selection_sort_blocks
   ( RandItKeys keys
   , typename iterator_traits<RandIt>::size_type &midkey_idx //inout
   , KeyCompare key_comp
   , RandIt const first_block
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const n_blocks
   , Compare comp
   , bool use_first_element)
{
   typedef typename iterator_traits<RandIt>::size_type size_type ;
   size_type const back_midkey_idx = midkey_idx;
   typedef typename iterator_traits<RandIt>::size_type   size_type;
   typedef typename iterator_traits<RandIt>::value_type  value_type;

   //Nothing to sort if 0 or 1 blocks or all belong to the first ordered half
   if(n_blocks < 2 || back_midkey_idx >= n_blocks){
      return;
   }
   //One-past the position of the first untouched element of the second half
   size_type high_watermark = back_midkey_idx+1;
   BOOST_ASSERT(high_watermark <= n_blocks);

   //Sort by first element if left merging, last element otherwise
   size_type const reg_off = use_first_element ? 0u: l_block-1;

   for(size_type block=0; block < n_blocks-1; ++block){
      size_type min_block = block;
      //Since we are searching for the minimum value in two sorted halves:
      //Optimization 1: If block belongs to first half, don't waste time comparing elements of the first half.
      //Optimization 2: It is enough to compare until the first untouched element of the second half.
      high_watermark = size_type(max_value(block+2, high_watermark));
      BOOST_ASSERT(high_watermark <= n_blocks);
      for(size_type next_block = size_type(max_value(block+1, back_midkey_idx)); next_block < high_watermark; ++next_block){
         const value_type &v   = first_block[next_block*l_block+reg_off];
         const value_type &min = first_block[min_block*l_block+reg_off];
         if( comp(v, min) || (!comp(min, v) && key_comp(keys[next_block], keys[min_block])) ){
            min_block=next_block;
         }
      }

      if(min_block != block){
         BOOST_ASSERT(block >= back_midkey_idx || min_block >= back_midkey_idx);
         BOOST_ASSERT(min_block < high_watermark);
         //Update high watermark if n_blocks is not surpassed
         high_watermark += size_type((min_block + 1) != n_blocks && (min_block + 1) == high_watermark);
         BOOST_ASSERT(high_watermark <= n_blocks);
         boost::adl_move_swap_ranges(first_block+block*l_block, first_block+(block+1)*l_block, first_block+min_block*l_block);
         boost::adl_move_swap(keys[block], keys[min_block]);
         if(midkey_idx == block)
            midkey_idx = min_block;
         else if(midkey_idx == min_block)
            midkey_idx = block;
      }
   }
}

template<class RandIt, class Compare>
void stable_sort( RandIt first, RandIt last, Compare comp
                , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const len = size_type(last - first);
   size_type const half_len = len/2 + (len&1);
   if(std::size_t(xbuf.capacity() - xbuf.size()) >= half_len) {
      merge_sort(first, last, comp, xbuf.data()+xbuf.size());
   }
   else{
      slow_stable_sort(first, last, comp);
   }
}

template<class RandIt, class Comp>
void initialize_keys( RandIt first, RandIt last
                    , Comp comp
                    , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf)
{
   stable_sort(first, last, comp, xbuf);
}

template<class RandIt, class U>
void initialize_keys( RandIt first, RandIt last
                    , less
                    , U &)
{
   typedef typename iterator_traits<RandIt>::value_type value_type;
   std::size_t count = std::size_t(last - first);
   for(std::size_t i = 0; i != count; ++i){
      *first = value_type(i);
      ++first;
   }
}

template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void combine_params
   ( RandItKeys const keys
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type l_combined
   , typename iterator_traits<RandIt>::size_type const l_prev_merged
   , typename iterator_traits<RandIt>::size_type const l_block
   , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
   , Compare comp
   //Output
   , typename iterator_traits<RandIt>::size_type &midkey_idx
   , typename iterator_traits<RandIt>::size_type &l_irreg1
   , typename iterator_traits<RandIt>::size_type &n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type &n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type &l_irreg2
   , bool is_merge_left)
{
   typedef typename iterator_traits<RandIt>::size_type   size_type;
   typedef typename iterator_traits<RandIt>::value_type  value_type;

   l_irreg1 = l_prev_merged%l_block;
   l_irreg2 = (l_combined-l_irreg1)%l_block;
   BOOST_ASSERT(((l_combined-l_irreg1-l_irreg2)%l_block) == 0);
   size_type const n_reg_block = (l_combined-l_irreg1-l_irreg2)/l_block;
   midkey_idx = l_prev_merged/l_block;
   BOOST_ASSERT(n_reg_block>=midkey_idx);
   initialize_keys(keys, keys+n_reg_block+(midkey_idx==n_reg_block), key_comp, xbuf);
   selection_sort_blocks(keys, midkey_idx, key_comp, first+l_irreg1, l_block, n_reg_block, comp, is_merge_left);

   n_aft_irreg2  = 0;
   if(l_irreg2!=0){
      size_type const reg_off   = is_merge_left ? 0u: l_block-1;
      size_type const irreg_off = is_merge_left ? 0u: l_irreg2-1;
      RandIt prev_block_first = first + l_combined - l_irreg2;
      const value_type &incomplete_block_first = prev_block_first[irreg_off];
		while(n_aft_irreg2 != n_reg_block && 
            comp(incomplete_block_first, (prev_block_first-= l_block)[reg_off]) ){
         ++n_aft_irreg2;
      }
   }
   n_bef_irreg2 = n_reg_block-n_aft_irreg2;
}

// first - first element to merge.
// first[-l_block, 0) - buffer (if use_buf == true)
// l_block - length of regular blocks. First nblocks are stable sorted by 1st elements and key-coded
// keys - sequence of keys, in same order as blocks. key<midkey means stream A
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be combined after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void merge_blocks_left
   ( RandItKeys const key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , bool const xbuf_used)
{
   if(xbuf_used){
      op_merge_blocks_left
         (key_first, midkey, key_comp, first, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, move_op());
   }
   else{
      op_merge_blocks_left
         (key_first, midkey, key_comp, first, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, swap_op());
   }
}


// first - first element to merge.
// [first+l_block*(n_bef_irreg2+n_aft_irreg2)+l_irreg2, first+l_block*(n_bef_irreg2+n_aft_irreg2+1)+l_irreg2) - buffer
// l_block - length of regular blocks. First nblocks are stable sorted by 1st elements and key-coded
// keys - sequence of keys, in same order as blocks. key<midkey means stream A
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be combined after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void merge_blocks_right
   ( RandItKeys const key_first
   , const typename iterator_traits<RandItKeys>::value_type &midkey
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const n_bef_irreg2
   , typename iterator_traits<RandIt>::size_type const n_aft_irreg2
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , bool const xbuf_used)
{
   if(xbuf_used){
      op_merge_blocks_right
         (key_first, midkey, key_comp, first, l_block, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, move_op());
   }
   else{
      op_merge_blocks_right
         (key_first, midkey, key_comp, first, l_block, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, swap_op());
   }
}


template<class RandIt>
void move_data_backward( RandIt cur_pos
              , typename iterator_traits<RandIt>::size_type const l_data
              , RandIt new_pos
              , bool const xbuf_used)
{
   //Move buffer to the total combination right
   if(xbuf_used){
      boost::move_backward(cur_pos, cur_pos+l_data, new_pos+l_data);      
   }
   else{
      boost::adl_move_swap_ranges_backward(cur_pos, cur_pos+l_data, new_pos+l_data);      
      //Rotate does less moves but it seems slower due to cache issues
      //rotate_gcd(first-l_block, first+len-l_block, first+len);
   }
}

template<class RandIt>
void move_data_forward( RandIt cur_pos
              , typename iterator_traits<RandIt>::size_type const l_data
              , RandIt new_pos
              , bool const xbuf_used)
{
   //Move buffer to the total combination right
   if(xbuf_used){
      boost::move(cur_pos, cur_pos+l_data, new_pos);
   }
   else{
      boost::adl_move_swap_ranges(cur_pos, cur_pos+l_data, new_pos);
      //Rotate does less moves but it seems slower due to cache issues
      //rotate_gcd(first-l_block, first+len-l_block, first+len);
   }
}

template <class Unsigned>
Unsigned calculate_total_combined(Unsigned const len, Unsigned const l_prev_merged, Unsigned *pl_irreg_combined = 0)
{
   typedef Unsigned size_type;

   size_type const l_combined = 2*l_prev_merged;
   size_type l_irreg_combined = len%l_combined;
   size_type l_total_combined = len;
   if(l_irreg_combined <= l_prev_merged){
      l_total_combined -= l_irreg_combined;
      l_irreg_combined = 0;
   }
   if(pl_irreg_combined)
      *pl_irreg_combined = l_irreg_combined;
   return l_total_combined;
}

// keys are on the left of first:
//    If use_buf: [first - l_block - n_keys, first - l_block).
//    Otherwise:  [first - n_keys, first).
// Buffer (if use_buf) is also on the left of first [first - l_block, first).
// Blocks of length l_prev_merged combined. We'll combine them in pairs
// l_prev_merged and n_keys are powers of 2. (2*l_prev_merged/l_block) keys are guaranteed
// Returns the number of combined elements (some trailing elements might be left uncombined)
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void combine_blocks
   ( RandItKeys const keys
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const len
   , typename iterator_traits<RandIt>::size_type const l_prev_merged
   , typename iterator_traits<RandIt>::size_type const l_block
   , bool const use_buf
   , bool const xbuf_used
   , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
   , Compare comp
   , bool merge_left)
{
   typedef typename iterator_traits<RandIt>::size_type   size_type;

   size_type const l_combined   = 2*l_prev_merged;
   size_type l_irreg_combined = 0;
   size_type const l_total_combined = calculate_total_combined(len, l_prev_merged, &l_irreg_combined);
   size_type const n_reg_combined = len/l_combined;
   RandIt combined_first = first;

   (void)l_total_combined;
   BOOST_ASSERT(l_total_combined <= len);

   size_type n_bef_irreg2, n_aft_irreg2, midkey_idx, l_irreg1, l_irreg2;
   size_type const max_i = n_reg_combined + (l_irreg_combined != 0);

   if(merge_left || !use_buf)
      for( size_type combined_i = 0; combined_i != max_i; ++combined_i, combined_first += l_combined) {
         bool const is_last = combined_i==n_reg_combined;
         combine_params( keys, key_comp, combined_first, is_last ? l_irreg_combined : l_combined
                       , l_prev_merged, l_block, xbuf, comp
                       , midkey_idx, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, true);   //Outputs
         //BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After combine_params: ", len + l_block);
         BOOST_ASSERT(!l_irreg1);
         if(use_buf){
            merge_blocks_left
               (keys, keys[midkey_idx], key_comp, combined_first, l_block, 0u, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, xbuf_used);
         }
         else{
            merge_blocks_bufferless
               (keys, keys[midkey_idx], key_comp, combined_first, l_block, 0u, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp);
         }
         //BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After merge_blocks_l: ", len + l_block);
      }
   else{
      combined_first += l_combined*(max_i-1);
      for( size_type combined_i = max_i; combined_i--; combined_first -= l_combined) {
         bool const is_last = combined_i==n_reg_combined;
         combine_params( keys, key_comp, combined_first, is_last ? l_irreg_combined : l_combined
                       , l_prev_merged, l_block, xbuf, comp
                       , midkey_idx, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, false);  //Outputs
         BOOST_ASSERT(!l_irreg1);
         //BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After combine_params: ", len + l_block);
         merge_blocks_right
            (keys, keys[midkey_idx], key_comp, combined_first, l_block, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, xbuf_used);
         //BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After merge_blocks_r: ", len + l_block);
      }
   }
}


template<class RandIt, class Compare>
typename iterator_traits<RandIt>::size_type
   buffered_merge_blocks
      ( RandIt const first, RandIt const last
      , typename iterator_traits<RandIt>::size_type const input_combined_size
      , Compare comp
      , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> &xbuf)
{
   typedef typename iterator_traits<RandIt>::size_type   size_type;
   size_type combined_size = input_combined_size;

   for( size_type const elements_in_blocks = size_type(last - first)
      ; elements_in_blocks > combined_size && size_type(xbuf.capacity()) >= combined_size
      ; combined_size *=2){
      RandIt merge_point = first;
      while(size_type(last - merge_point) > 2*combined_size) {
         RandIt const second_half = merge_point+combined_size;
         RandIt const next_merge_point = second_half+combined_size;
         buffered_merge(merge_point, second_half, next_merge_point, comp, xbuf);
         merge_point = next_merge_point;
      }
      if(size_type(last-merge_point) > combined_size){
         buffered_merge(merge_point, merge_point+combined_size, last, comp, xbuf);
      }
   }
   return combined_size;
}

template<class RandIt, class Compare, class Op>
typename iterator_traits<RandIt>::size_type
   op_insertion_sort_step_left
      ( RandIt const first
      , typename iterator_traits<RandIt>::size_type const length
      , typename iterator_traits<RandIt>::size_type const step
      , Compare comp, Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const s = min_value<size_type>(step, AdaptiveSortInsertionSortThreshold);
   size_type m = 0;

   while((length - m) > s){
      insertion_sort_op(first+m, first+m+s, first+m-s, comp, op);
      m += s;
   }
   insertion_sort_op(first+m, first+length, first+m-s, comp, op);
   return s;
}

template<class RandIt, class Compare>
typename iterator_traits<RandIt>::size_type
   insertion_sort_step
      ( RandIt const first
      , typename iterator_traits<RandIt>::size_type const length
      , typename iterator_traits<RandIt>::size_type const step
      , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const s = min_value<size_type>(step, AdaptiveSortInsertionSortThreshold);
   size_type m = 0;

   while((length - m) > s){
      insertion_sort(first+m, first+m+s, comp);
      m += s;
   }
   insertion_sort(first+m, first+length, comp);
   return s;
}

template<class RandIt, class Compare, class Op>
typename iterator_traits<RandIt>::size_type  
   op_merge_left_step
      ( RandIt first_block
      , typename iterator_traits<RandIt>::size_type const elements_in_blocks
      , typename iterator_traits<RandIt>::size_type l_merged
      , typename iterator_traits<RandIt>::size_type const l_build_buf
      , typename iterator_traits<RandIt>::size_type l_left_space
      , Compare comp
      , Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   for(; l_merged < l_build_buf && l_left_space >= l_merged; l_merged*=2){
      size_type p0=0;
      RandIt pos = first_block;
      while((elements_in_blocks - p0) > 2*l_merged) {
         op_merge_left(pos-l_merged, pos, pos+l_merged, pos+2*l_merged, comp, op);
         p0 += 2*l_merged;
         pos = first_block+p0;
      }
      if((elements_in_blocks-p0) > l_merged) {
         op_merge_left(pos-l_merged, pos, pos+l_merged, first_block+elements_in_blocks, comp, op);
      }
      else {
         op(forward_t(), pos, first_block+elements_in_blocks, pos-l_merged);
      }
      first_block -= l_merged;
      l_left_space -= l_merged;
   }
   return l_merged;
}

template<class RandIt, class Compare, class Op>
void op_merge_right_step
      ( RandIt first_block
      , typename iterator_traits<RandIt>::size_type const elements_in_blocks
      , typename iterator_traits<RandIt>::size_type const l_build_buf
      , Compare comp
      , Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type restk = elements_in_blocks%(2*l_build_buf);
   size_type p = elements_in_blocks - restk;
   BOOST_ASSERT(0 == (p%(2*l_build_buf)));

   if(restk <= l_build_buf){
      op(backward_t(),first_block+p, first_block+p+restk, first_block+p+restk+l_build_buf);
   }
	else{
      op_merge_right(first_block+p, first_block+p+l_build_buf, first_block+p+restk, first_block+p+restk+l_build_buf, comp, op);
   }
   while(p>0){
      p -= 2*l_build_buf;
		op_merge_right(first_block+p, first_block+p+l_build_buf, first_block+p+2*l_build_buf, first_block+p+3*l_build_buf, comp, op);
   }
}


// build blocks of length 2*l_build_buf. l_build_buf is power of two
// input: [0, l_build_buf) elements are buffer, rest unsorted elements
// output: [0, l_build_buf) elements are buffer, blocks 2*l_build_buf and last subblock sorted
//
// First elements are merged from right to left until elements start
// at first. All old elements [first, first + l_build_buf) are placed at the end
// [first+len-l_build_buf, first+len). To achieve this:
// - If we have external memory to merge, we save elements from the buffer
//   so that a non-swapping merge is used. Buffer elements are restored
//   at the end of the buffer from the external memory.
//
// - When the external memory is not available or it is insufficient
//   for a merge operation, left swap merging is used.
//
// Once elements are merged left to right in blocks of l_build_buf, then a single left
// to right merge step is performed to achieve merged blocks of size 2K.
// If external memory is available, usual merge is used, swap merging otherwise.
//
// As a last step, if auxiliary memory is available in-place merge is performed.
// until all is merged or auxiliary memory is not large enough.
template<class RandIt, class Compare>
typename iterator_traits<RandIt>::size_type  
   build_blocks
      ( RandIt const first
      , typename iterator_traits<RandIt>::size_type const len
      , typename iterator_traits<RandIt>::size_type const l_base
      , typename iterator_traits<RandIt>::size_type const l_build_buf
      , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
      , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   BOOST_ASSERT(l_build_buf <= len);
   BOOST_ASSERT(0 == ((l_build_buf / l_base)&(l_build_buf/l_base-1)));

   //Place the start pointer after the buffer
   RandIt first_block = first + l_build_buf;
   size_type const elements_in_blocks = len - l_build_buf;

   //////////////////////////////////
   // Start of merge to left step
   //////////////////////////////////
   size_type l_merged = 0u;

//   if(xbuf.capacity()>=2*l_build_buf){
   if(!l_build_buf){
      l_merged = insertion_sort_step(first_block, elements_in_blocks, l_base, comp);
      //2*l_build_buf already merged, now try to merge further
      //using classic in-place mergesort if enough auxiliary memory is available
      return buffered_merge_blocks
         (first_block, first_block + elements_in_blocks, l_merged, comp, xbuf);
   }
   else{
      //If there is no enough buffer for the insertion sort step, just avoid the external buffer
      size_type kbuf = min_value<size_type>(l_build_buf, size_type(xbuf.capacity()));
      kbuf = kbuf < l_base ? 0 : kbuf;

      if(kbuf){
         //Backup internal buffer values in external buffer so they can be overwritten
         xbuf.move_assign(first+l_build_buf-kbuf, kbuf);
         l_merged = op_insertion_sort_step_left(first_block, elements_in_blocks, l_base, comp, move_op());

         //Now combine them using the buffer. Elements from buffer can be
         //overwritten since they've been saved to xbuf
         l_merged = op_merge_left_step
            ( first_block - l_merged, elements_in_blocks, l_merged, l_build_buf, kbuf - l_merged, comp, move_op());

         //Restore internal buffer from external buffer unless kbuf was l_build_buf,
         //in that case restoration will happen later
         if(kbuf != l_build_buf){
            boost::move(xbuf.data()+kbuf-l_merged, xbuf.data() + kbuf, first_block-l_merged+elements_in_blocks);
         }
      }
      else{
         l_merged = insertion_sort_step(first_block, elements_in_blocks, l_base, comp);
         rotate_gcd(first_block - l_merged, first_block, first_block+elements_in_blocks);
      }

      //Now combine elements using the buffer. Elements from buffer can't be
      //overwritten since xbuf was not big enough, so merge swapping elements.
      l_merged = op_merge_left_step
         (first_block - l_merged, elements_in_blocks, l_merged, l_build_buf, l_build_buf - l_merged, comp, swap_op());

      BOOST_ASSERT(l_merged == l_build_buf);

      //////////////////////////////////
      // Start of merge to right step
      //////////////////////////////////

      //If kbuf is l_build_buf then we can merge right without swapping
      //Saved data is still in xbuf
      if(kbuf && kbuf == l_build_buf){
         op_merge_right_step(first, elements_in_blocks, l_build_buf, comp, move_op());
         //Restore internal buffer from external buffer if kbuf was l_build_buf.
         //as this operation was previously delayed.
         boost::move(xbuf.data(), xbuf.data() + kbuf, first);
      }
      else{
         op_merge_right_step(first, elements_in_blocks, l_build_buf, comp, swap_op());
      }
      xbuf.clear();
      //2*l_build_buf already merged, now try to merge further
      //using classic in-place mergesort if enough auxiliary memory is available
      return buffered_merge_blocks
         (first_block, first_block + elements_in_blocks, l_build_buf*2, comp, xbuf);
   }
}

//Returns true if buffer is placed in 
//[buffer+len-l_intbuf, buffer+len). Otherwise, buffer is
//[buffer,buffer+l_intbuf)
template<class RandIt, class Compare>
bool combine_all_blocks
   ( RandIt keys
   , typename iterator_traits<RandIt>::size_type &n_keys
   , RandIt const buffer
   , typename iterator_traits<RandIt>::size_type const l_buf_plus_data
   , typename iterator_traits<RandIt>::size_type l_merged
   , typename iterator_traits<RandIt>::size_type &l_intbuf
   , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
   , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   RandIt const first = buffer + l_intbuf;
   size_type const l_data = l_buf_plus_data - l_intbuf;
   size_type const l_unique = l_intbuf+n_keys;
   //Backup data to external buffer once if possible
   bool const common_xbuf = l_data > l_merged && l_intbuf && l_intbuf <= xbuf.capacity();
   if(common_xbuf){
      if(n_keys){
         xbuf.move_assign(buffer, l_intbuf);
      }
      else{
         xbuf.clear();
         merge_sort_uninitialized_copy(buffer, first, xbuf.data(), comp);
         xbuf.set_size(l_intbuf);
      }
   }

   bool prev_merge_left = true;
   size_type l_prev_total_combined = 0u, l_prev_block = 0;
   bool prev_use_internal_buf = true;

   for( size_type n = 0; l_data > l_merged
      ; l_merged*=2
      , ++n){
      //If l_intbuf is non-zero, use that internal buffer.
      //    Implies l_block == l_intbuf && use_internal_buf == true
      //If l_intbuf is zero, see if half keys can be reused as a reduced emergency buffer,
      //    Implies l_block == n_keys/2 && use_internal_buf == true
      //Otherwise, just give up and and use all keys to merge using rotations (use_internal_buf = false)
      bool use_internal_buf = false;
      size_type const l_block = lblock_for_combine(l_intbuf, n_keys, 2*l_merged, use_internal_buf);
      BOOST_ASSERT(!l_intbuf || (l_block == l_intbuf));
      BOOST_ASSERT(n == 0 || (!use_internal_buf || prev_use_internal_buf) );
      BOOST_ASSERT(n == 0 || (!use_internal_buf || l_prev_block == l_block) );
      
      bool const is_merge_left = (n&1) == 0;
      size_type const l_total_combined = calculate_total_combined(l_data, l_merged);

      if(prev_use_internal_buf && prev_merge_left){
         if(is_merge_left || !use_internal_buf){
            move_data_backward(first-l_prev_block, l_prev_total_combined, first, common_xbuf);
         }
         else{
            //Put the buffer just after l_total_combined
            RandIt const buf_end = first+l_prev_total_combined;
            RandIt const buf_beg = buf_end-l_block;
            if(l_prev_total_combined > l_total_combined){
               size_type const l_diff = l_prev_total_combined - l_total_combined;
               move_data_backward(buf_beg-l_diff, l_diff, buf_end-l_diff, common_xbuf);
            }
            else if(l_prev_total_combined < l_total_combined){
               size_type const l_diff = l_total_combined - l_prev_total_combined;
               move_data_forward(buf_end, l_diff, buf_beg, common_xbuf);
            }
         }
      }
      BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After move_data     : ", l_data + l_intbuf);

      //Combine to form l_merged*2 segments
      if(n_keys){
         combine_blocks
            ( keys, comp, !use_internal_buf || is_merge_left ? first : first-l_block
            , l_data, l_merged, l_block, use_internal_buf, common_xbuf, xbuf, comp, is_merge_left);
      }
      else{
         size_type *const uint_keys = xbuf.template aligned_trailing<size_type>();
         combine_blocks
            ( uint_keys, less(), !use_internal_buf || is_merge_left ? first : first-l_block
            , l_data, l_merged, l_block, use_internal_buf, common_xbuf, xbuf, comp, is_merge_left);
      }
      BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After combine_blocks: ", l_data + l_intbuf);
      prev_merge_left = is_merge_left;
      l_prev_total_combined = l_total_combined;
      l_prev_block = l_block;
      prev_use_internal_buf = use_internal_buf;
   }
   BOOST_ASSERT(l_prev_total_combined == l_data);
   bool const buffer_right = prev_use_internal_buf && prev_merge_left;
   l_intbuf = prev_use_internal_buf ? l_prev_block : 0u;
   n_keys = l_unique - l_intbuf;
   //Restore data from to external common buffer if used
   if(common_xbuf){
      if(buffer_right){
         boost::move(xbuf.data(), xbuf.data() + l_intbuf, buffer+l_data);
      }
      else{
         boost::move(xbuf.data(), xbuf.data() + l_intbuf, buffer);
      }
   }
   return buffer_right;
}

template<class RandIt, class Compare>
void stable_merge
      ( RandIt first, RandIt const middle, RandIt last
      , Compare comp
      , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> &xbuf)
{
   BOOST_ASSERT(xbuf.empty());
   typedef typename iterator_traits<RandIt>::size_type   size_type;
   size_type const len1  = size_type(middle-first);
   size_type const len2  = size_type(last-middle);
   size_type const l_min = min_value(len1, len2);
   if(xbuf.capacity() >= l_min){
      buffered_merge(first, middle, last, comp, xbuf);
      xbuf.clear();
   }
   else{
      merge_bufferless(first, middle, last, comp);
   }
}


template<class RandIt, class Compare>
void final_merge( bool buffer_right
                , RandIt const first
                , typename iterator_traits<RandIt>::size_type const l_intbuf
                , typename iterator_traits<RandIt>::size_type const n_keys
                , typename iterator_traits<RandIt>::size_type const len
                , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
                , Compare comp)
{
   BOOST_ASSERT(n_keys || xbuf.size() == l_intbuf);
   xbuf.clear();

   typedef typename iterator_traits<RandIt>::size_type  size_type;
   size_type const n_key_plus_buf = l_intbuf+n_keys;
   if(buffer_right){
      stable_sort(first+len-l_intbuf, first+len, comp, xbuf);
      stable_merge(first+n_keys, first+len-l_intbuf, first+len, antistable<Compare>(comp), xbuf);
      stable_sort(first, first+n_keys, comp, xbuf);
      stable_merge(first, first+n_keys, first+len, comp, xbuf);
   }
   else{
      stable_sort(first, first+n_key_plus_buf, comp, xbuf);
      if(xbuf.capacity() >= n_key_plus_buf){
         buffered_merge(first, first+n_key_plus_buf, first+len, comp, xbuf);
      }
      else if(xbuf.capacity() >= min_value<size_type>(l_intbuf, n_keys)){
         stable_merge(first+n_keys, first+n_key_plus_buf, first+len, comp, xbuf);
         stable_merge(first, first+n_keys, first+len, comp, xbuf);
      }
      else{
         merge_bufferless(first, first+n_key_plus_buf, first+len, comp);
      }
   }
   BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After final_merge   : ", len);
}

template<class RandIt, class Compare, class Unsigned, class T>
bool build_params
   (RandIt first, Unsigned const len, Compare comp
   , Unsigned &n_keys, Unsigned &l_intbuf, Unsigned &l_base, Unsigned &l_build_buf
   , adaptive_xbuf<T> & xbuf
   )
{
   typedef Unsigned size_type;

   //Calculate ideal parameters and try to collect needed unique keys
   l_base = 0u;

   //Try to find a value near sqrt(len) that is 2^N*l_base where
   //l_base <= AdaptiveSortInsertionSortThreshold. This property is important
   //as build_blocks merges to the left iteratively duplicating the
   //merged size and all the buffer must be used just before the final
   //merge to right step. This guarantees "build_blocks" produces 
   //segments of size l_build_buf*2, maximizing the classic merge phase.
   l_intbuf = size_type(ceil_sqrt_multiple(len, &l_base));

   //This is the minimum number of case to implement the ideal algorithm
   //
   //l_intbuf is used as buffer plus the key count
   size_type n_min_ideal_keys = l_intbuf-1u;
   while(n_min_ideal_keys >= (len-l_intbuf-n_min_ideal_keys)/l_intbuf){
      --n_min_ideal_keys;
   }
   ++n_min_ideal_keys;
   BOOST_ASSERT(n_min_ideal_keys < l_intbuf);

   if(xbuf.template supports_aligned_trailing<size_type>(l_intbuf, n_min_ideal_keys)){
      n_keys = 0u;
      l_build_buf = l_intbuf;
   }
   else{
      //Try to achieve a l_build_buf of length l_intbuf*2, so that we can merge with that
      //l_intbuf*2 buffer in "build_blocks" and use half of them as buffer and the other half
      //as keys in combine_all_blocks. In that case n_keys >= n_min_ideal_keys but by a small margin.
      //
      //If available memory is 2*sqrt(l), then only sqrt(l) unique keys are needed,
      //(to be used for keys in combine_all_blocks) as the whole l_build_buf
      //we'll be backuped in the buffer during build_blocks.
      bool const non_unique_buf = xbuf.capacity() >= 2*l_intbuf;
      size_type const to_collect = non_unique_buf ? l_intbuf : l_intbuf*2;
	   size_type collected = collect_unique(first, first+len, to_collect, comp, xbuf);

      //If available memory is 2*sqrt(l), then for "build_params" 
      //the situation is the same as if 2*l_intbuf were collected.
      if(non_unique_buf && (collected >= n_min_ideal_keys))
         collected += l_intbuf;

      //If collected keys are not enough, try to fix n_keys and l_intbuf. If no fix
      //is possible (due to very low unique keys), then go to a slow sort based on rotations.
      if(collected < (n_min_ideal_keys+l_intbuf)){
         if(collected < 4){  //No combination possible with less that 4 keys
			   return false;
         }
         n_keys = l_intbuf;
         while(n_keys&(n_keys-1)){
            n_keys &= n_keys-1;  // make it power or 2
         }
         while(n_keys > collected){
            n_keys/=2;
         }
         l_base = min_value<Unsigned>(n_keys, AdaptiveSortInsertionSortThreshold);
         l_intbuf = 0;
         l_build_buf = n_keys;
      }
      else if((collected - l_intbuf) >= l_intbuf){
         //l_intbuf*2 elements found. Use all of them in the build phase 
         l_build_buf = l_intbuf*2;
         n_keys = l_intbuf;
      }
      else{
         l_build_buf = l_intbuf;
         n_keys = n_min_ideal_keys;
      }
      BOOST_ASSERT((n_keys+l_intbuf) >= l_build_buf);
   }

   return true;
}

template<class RandIt, class Compare>
void adaptive_sort_impl
   ( RandIt first
   , typename iterator_traits<RandIt>::size_type const len
   , Compare comp
   , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
   )
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;

   //Small sorts go directly to insertion sort
   if(len <= size_type(AdaptiveSortInsertionSortThreshold)){
      insertion_sort(first, first + len, comp);
      return;
   }
   
   if((len-len/2) <= xbuf.capacity()){
      merge_sort(first, first+len, comp, xbuf.data());
      return;
   }

   //Make sure it is at least two
   BOOST_STATIC_ASSERT(AdaptiveSortInsertionSortThreshold >= 4);

   size_type l_base = 0;
   size_type l_intbuf = 0;
   size_type n_keys = 0;
   size_type l_build_buf = 0;

   if(!build_params(first, len, comp, n_keys, l_intbuf, l_base, l_build_buf, xbuf)){
      stable_sort(first, first+len, comp, xbuf);
      return;
   }

   //Otherwise, continue in adaptive_sort
   BOOST_MOVE_ADAPTIVE_SORT_PRINT("\n   After collect_unique: ", len);
   size_type const n_key_plus_buf = l_intbuf+n_keys;
   //l_build_buf is always power of two if l_intbuf is zero
   BOOST_ASSERT(l_intbuf || (0 == (l_build_buf & (l_build_buf-1))));

   //Classic merge sort until internal buffer and xbuf are exhausted
   size_type const l_merged = build_blocks
      (first+n_key_plus_buf-l_build_buf, len-n_key_plus_buf+l_build_buf, l_base, l_build_buf, xbuf, comp);
   BOOST_MOVE_ADAPTIVE_SORT_PRINT("   After build_blocks:   ", len);

   //Non-trivial merge
   bool const buffer_right = combine_all_blocks
      (first, n_keys, first+n_keys, len-n_keys, l_merged, l_intbuf, xbuf, comp);

   //Sort keys and buffer and merge the whole sequence
   final_merge(buffer_right, first, l_intbuf, n_keys, len, xbuf, comp);
}

template<class RandIt, class Compare>
void adaptive_merge_impl
   ( RandIt first
   , typename iterator_traits<RandIt>::size_type const len1
   , typename iterator_traits<RandIt>::size_type const len2
   , Compare comp
   , adaptive_xbuf<typename iterator_traits<RandIt>::value_type> & xbuf
   )
{
   typedef typename iterator_traits<RandIt>::size_type size_type;

   if(xbuf.capacity() >= min_value<size_type>(len1, len2)){
      buffered_merge(first, first+len1, first+(len1+len2), comp, xbuf);
   }
   else{
      const size_type len = len1+len2;
      //Calculate ideal parameters and try to collect needed unique keys
      size_type l_block = size_type(ceil_sqrt(len));

      if(len1 <= l_block*2 || len2 <= l_block*2){
         merge_bufferless(first, first+len1, first+len1+len2, comp);
         return;
      }

      size_type l_intbuf = xbuf.capacity() >= l_block ? 0u : l_block;

      //This is the minimum number of case to implement the ideal algorithm
      //ceil(len/l_block) - 1 (as the first block is used as buffer)
      size_type n_keys = l_block;
      while(n_keys >= (len-l_intbuf-n_keys)/l_block){
         --n_keys;
      }
      ++n_keys;
      BOOST_ASSERT(n_keys < l_block);

      if(xbuf.template supports_aligned_trailing<size_type>(l_block, n_keys)){
         n_keys = 0u;
      }

      size_type const to_collect = l_intbuf+n_keys;
	   size_type const collected = collect_unique(first, first+len1, to_collect, comp, xbuf);

      BOOST_MOVE_ADAPTIVE_SORT_PRINT("\n   A collect: ", len);
      if(collected != to_collect && collected < 4){
         merge_bufferless(first, first+len1, first+len1+len2, comp);
      }
      else{
         bool use_internal_buf = true;
         if (collected != to_collect){
            l_intbuf = 0u;
            n_keys = collected;
            use_internal_buf = false;
            l_block  = lblock_for_combine(l_intbuf, n_keys, len, use_internal_buf);
            l_intbuf = use_internal_buf ? l_block : 0u;
         }

         bool xbuf_used = collected == to_collect && xbuf.capacity() >= l_block;
         size_type const l_combine  = len-collected;
         size_type const l_combine1 = len1-collected;

         size_type n_bef_irreg2, n_aft_irreg2, l_irreg1, l_irreg2, midkey_idx;
         if(n_keys){
            RandIt const first_data = first+collected;
            RandIt const keys = first;
            combine_params( keys, comp, first_data, l_combine
                          , l_combine1, l_block, xbuf, comp
                          , midkey_idx, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, true);   //Outputs
            BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A combine: ", len);
            if(xbuf_used){
               merge_blocks_with_buf
                  (keys, keys[midkey_idx], comp, first_data, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, xbuf, xbuf_used);
               BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A mrg xbf: ", len);
            }
            else if(use_internal_buf){
               #define BOOST_MOVE_ADAPTIVE_MERGE_WITH_BUF
               #ifdef BOOST_MOVE_ADAPTIVE_MERGE_WITH_BUF
               range_xbuf<RandIt, swap_op> rbuf(first_data-l_block, first_data);
               merge_blocks_with_buf
                  (keys, keys[midkey_idx], comp, first_data, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, rbuf, xbuf_used);
               BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A mrg buf: ", len);
               #else
               merge_blocks_left
                  (keys, keys[midkey_idx], comp, first_data, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, xbuf_used);
               BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A mrg lft: ", len);
               #endif
            }
            else{
               merge_blocks_bufferless
                  (keys, keys[midkey_idx], comp, first_data, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp);
               BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A mrg bfl: ", len);
            }
         }
         else{
            xbuf.clear();
            size_type *const uint_keys = xbuf.template aligned_trailing<size_type>(l_block);
            combine_params( uint_keys, less(), first, l_combine
                          , l_combine1, l_block, xbuf, comp
                          , midkey_idx, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, true);   //Outputs
            BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A combine: ", len);
            merge_blocks_with_buf
               (uint_keys, uint_keys[midkey_idx], less(), first, l_block, l_irreg1, n_bef_irreg2, n_aft_irreg2, l_irreg2, comp, xbuf, true);
            xbuf.clear();
            BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A mrg lft: ", len);
         }

         n_keys = collected-l_intbuf;
         if(use_internal_buf){
            if(xbuf_used){
               xbuf.clear();
               //Nothing to do
               if(n_keys){
                  stable_sort(first, first+n_keys, comp, xbuf);
                  stable_merge(first, first+n_keys, first+len, comp, xbuf);
               }
            }
            else{
               #ifdef BOOST_MOVE_ADAPTIVE_MERGE_WITH_BUF
               xbuf.clear();
               stable_sort(first, first+collected, comp, xbuf);
               stable_merge(first, first+collected, first+len, comp, xbuf);
               #else
               xbuf.clear();
               stable_sort(first+len-l_block, first+len, comp, xbuf);
               RandIt const pos1 = lower_bound(first+n_keys, first+len-l_block, first[len-1], comp);
               RandIt const pos2 = rotate_gcd(pos1, first+len-l_block, first+len);
               stable_merge(first+n_keys, pos1, pos2, antistable<Compare>(comp), xbuf);
               if(n_keys){
                  stable_sort(first, first+n_keys, comp, xbuf);
                  stable_merge(first, first+n_keys, first+len, comp, xbuf);
               }
               #endif
            }

            BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A buf mrg: ", len);
         }
         else{
            stable_sort(first, first+collected, comp, xbuf);
            xbuf.clear();
            if(xbuf.capacity() >= collected){
               buffered_merge(first, first+collected, first+len1+len2, comp, xbuf);
            }
            else{
               merge_bufferless(first, first+collected, first+len1+len2, comp);
            }
         }
         BOOST_MOVE_ADAPTIVE_SORT_PRINT("   A key mrg: ", len);
      }
   }
}

}  //namespace detail_adaptive {
}  //namespace movelib {
}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif   //#define BOOST_MOVE_ADAPTIVE_SORT_MERGE_HPP
