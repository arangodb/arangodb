////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////
/// @class output_stream_iterator
/// @brief lightweight single pass `OutputIterator` that writes successive
///        characters into underlying stream
//////////////////////////////////////////////////////////////////////////////
template<typename Stream>
class output_stream_iterator
    : public std::iterator<std::output_iterator_tag, void, void, void, void> {
  public:
   explicit output_stream_iterator(Stream& strm) NOEXCEPT
     : strm_(&strm) {
   }

    output_stream_iterator& operator*() NOEXCEPT { return *this; }
    output_stream_iterator& operator++() NOEXCEPT { return *this; }
    output_stream_iterator& operator++(int) NOEXCEPT { return *this; }

    template<typename T>
    output_stream_iterator& operator=(const T& value) {
      strm_->put(value);
      return *this;
    }

  private:
   Stream* strm_;
};

template<typename Stream>
output_stream_iterator<Stream> make_ostream_iterator(Stream& strm) {
  return output_stream_iterator<Stream>(strm);
}

//////////////////////////////////////////////////////////////////////////////
/// @class input_stream_iterator
/// @brief lightweight single pass `InputIterator` that reads successive
///        characters from underlying stream
//////////////////////////////////////////////////////////////////////////////
template<typename Stream>
class input_stream_iterator
    : public std::iterator<std::input_iterator_tag, typename Stream::char_type> {
  public:
   typedef typename Stream::int_type int_type;

   explicit input_stream_iterator(Stream& strm) NOEXCEPT
     : strm_(&strm) {
   }

    int_type operator*() const NOEXCEPT {
      return strm_->get();
    }

    input_stream_iterator& operator++() NOEXCEPT { return *this; }
    input_stream_iterator& operator++(int) NOEXCEPT { return *this; }

  private:
   Stream* strm_;
}; // input_stream_iterator

template<typename Stream>
input_stream_iterator<Stream> make_istream_iterator(Stream& strm) {
  return input_stream_iterator<Stream>(strm);
}

NS_END
NS_END

#endif
