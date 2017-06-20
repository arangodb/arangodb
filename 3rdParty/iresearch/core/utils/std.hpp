//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_STD_H
#define IRESEARCH_STD_H

#include "shared.hpp"

#include <iterator>
#include <algorithm>

NS_ROOT

NS_BEGIN(irstd)

// constexpr versions of min/max functions (for prior c++14 environments)
template<typename T> 
CONSTEXPR const T& (max)(const T& a, const T& b) { 
  return a < b ? b : a; 
}

template<typename T> 
CONSTEXPR const T& (min)(const T& a, const T& b) { 
  return a < b ? a : b; 
}

// converts reverse iterator to corresponding forward iterator
// the function does not accept containers "end"
template<typename ReverseIterator>
CONSTEXPR typename ReverseIterator::iterator_type to_forward(ReverseIterator it) {
  return --(it.base());
}

template<typename Iterator>
CONSTEXPR std::reverse_iterator<Iterator> make_reverse_iterator(Iterator it) {
  return std::reverse_iterator<Iterator>(it);
}

NS_BEGIN(heap)
NS_BEGIN(detail)

template<typename RandomAccessIterator, typename Diff, typename Func >
inline void _for_each_top(
    RandomAccessIterator begin,
    Diff idx, Diff bottom,
    Func func) {
  if (idx < bottom) {
    const RandomAccessIterator target = begin + idx;
    if (*begin == *target) {
      func(*target);
      _for_each_top(begin, 2 * idx + 1, bottom, func);
      _for_each_top(begin, 2 * idx + 2, bottom, func);
    }
  }
}

template<
  typename RandomAccessIterator, typename Diff,
  typename Pred, typename Func>
inline void _for_each_if(
    RandomAccessIterator begin,
    Diff idx, Diff bottom,
    Pred pred, Func func) {
  if (idx < bottom) {
    const RandomAccessIterator target = begin + idx;
    if (pred(*target)) {
      func(*target);
      _for_each_if(begin, 2 * idx + 1, bottom, pred, func);
      _for_each_if(begin, 2 * idx + 2, bottom, pred, func);
    }
  }
}

NS_END // detail

/////////////////////////////////////////////////////////////////////////////
/// @brief calls func for each element in a heap equals to top
////////////////////////////////////////////////////////////////////////////
template<typename RandomAccessIterator, typename Pred, typename Func>
inline void for_each_if(
    RandomAccessIterator begin,
    RandomAccessIterator end,
    Pred pred,
    Func func) {
  typedef typename std::iterator_traits<
      RandomAccessIterator
  >::difference_type difference_type;

  detail::_for_each_if(
      begin,
      difference_type(0),
      difference_type(end - begin),
      pred, func);
}

/////////////////////////////////////////////////////////////////////////////
/// @brief calls func for each element in a heap equals to top
////////////////////////////////////////////////////////////////////////////
template< typename RandomAccessIterator, typename Func >
inline void for_each_top( 
    RandomAccessIterator begin, 
    RandomAccessIterator end, 
    Func func) {
  typedef typename std::iterator_traits<
    RandomAccessIterator
  >::difference_type difference_type;
  
  detail::_for_each_top( 
    begin, 
    difference_type(0),
    difference_type(end - begin),
    func);
}

NS_END // heap

/////////////////////////////////////////////////////////////////////////////
/// @brief checks that all values in the specified range are equals 
////////////////////////////////////////////////////////////////////////////
template<typename ForwardIterator>
inline bool all_equal(ForwardIterator begin, ForwardIterator end) {
  typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
  return end == std::adjacent_find(begin, end, std::not_equal_to<value_type>());
}

//////////////////////////////////////////////////////////////////////////////
/// @class back_emplace_iterator 
/// @brief provide in place construction capabilities for stl algorithms 
////////////////////////////////////////////////////////////////////////////
template<typename Container>
class back_emplace_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
 public:
  typedef Container container_type;

  explicit back_emplace_iterator(Container& cont) 
    : cont_(std::addressof(cont)) {
  }

  template<class T>
  back_emplace_iterator<Container>& operator=(T&& t) {
    cont_->emplace_back(std::forward<T>(t));
    return *this;
  }

  back_emplace_iterator& operator*() { return *this; }
  back_emplace_iterator& operator++() { return *this; }
  back_emplace_iterator& operator++(int) { return *this; }

 private:
  Container* cont_;
}; // back_emplace_iterator 

template<typename Container>
inline back_emplace_iterator<Container> back_emplacer(Container& cont) {
  return back_emplace_iterator<Container>(cont);
}

NS_END
NS_END

#endif
