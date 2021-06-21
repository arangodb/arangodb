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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_STD_H
#define IRESEARCH_STD_H

#include <iterator>
#include <algorithm>

#include "shared.hpp"

namespace iresearch {
namespace irstd {

template<typename In, typename Out>
struct adjust_const {
  typedef Out value_type;
  typedef Out& reference;
  typedef Out* pointer;
};

template<typename In, typename Out>
struct adjust_const<const In, Out> {
  typedef const Out value_type;
  typedef const Out& reference;
  typedef const Out* pointer;
};

// constexpr versions of min/max functions (for prior c++14 environments)
template<typename T> 
constexpr const T& (max)(const T& a, const T& b) { 
  return a < b ? b : a; 
}

template<typename T> 
constexpr const T& (min)(const T& a, const T& b) { 
  return a < b ? a : b; 
}

// converts reverse iterator to corresponding forward iterator
// the function does not accept containers "end"
template<typename ReverseIterator>
constexpr typename ReverseIterator::iterator_type to_forward(ReverseIterator it) {
  return --(it.base());
}

template<typename Iterator>
constexpr std::reverse_iterator<Iterator> make_reverse_iterator(Iterator it) {
  return std::reverse_iterator<Iterator>(it);
}

template<typename Container, typename Iterator>
void swap_remove(Container& cont, Iterator it) {
  assert(!cont.empty());
  std::swap(*it, cont.back());
  cont.pop_back();
}

namespace heap {
namespace detail {

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

} // detail

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

} // heap

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
class back_emplace_iterator {
 public:
  using container_type = Container;
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using difference_type = void;

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
class output_stream_iterator {
public:
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using difference_type = void;

  explicit output_stream_iterator(Stream& strm) noexcept
    : strm_(&strm) {
  }

  output_stream_iterator& operator*() noexcept { return *this; }
  output_stream_iterator& operator++() noexcept { return *this; }
  output_stream_iterator& operator++(int) noexcept { return *this; }

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
class input_stream_iterator {
public:
  using iterator_category = std::input_iterator_tag;
  using value_type = typename Stream::char_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;

  explicit input_stream_iterator(Stream& strm) noexcept
    : strm_(&strm) {
  }

  typename Stream::int_type operator*() const noexcept {
    return strm_->get();
  }

  input_stream_iterator& operator++() noexcept { return *this; }
  input_stream_iterator& operator++(int) noexcept { return *this; }

private:
  Stream* strm_;
}; // input_stream_iterator

template<typename Stream>
input_stream_iterator<Stream> make_istream_iterator(Stream& strm) {
  return input_stream_iterator<Stream>(strm);
}

namespace detail {

template<typename Builder, size_t Size>
struct initializer {
  using type = typename Builder::type;

  static constexpr auto Idx = Size - 1;

  template<typename Array>
  constexpr initializer(Array& cache) : init_(cache) {
    cache[Idx] = []() -> const type& {
      static const typename Builder::type INSTANCE
        = Builder::make(Idx);
      return INSTANCE;
    };
  }

  initializer<Builder, Size - 1> init_;
};

template<typename Builder>
struct initializer<Builder, 1> {
  using type = typename Builder::type;

  static constexpr auto Idx = 0;

  template<typename Array>
  constexpr initializer(Array& cache) {
    cache[Idx] = []() -> const type& {
      static const typename Builder::type INSTANCE
        = Builder::make(Idx);
      return INSTANCE;
    };
  }
};

template<typename Builder>
struct initializer<Builder, 0>;

} // detail

template<typename Builder, size_t Size>
class static_lazy_array {
 public:
  using type = typename Builder::type;

  static const type& at(size_t i) {
    static const static_lazy_array INSTANCE;
    return INSTANCE.cache_[std::min(i, Size)]();
  }

 private:
  constexpr static_lazy_array() : init_{cache_} {
    cache_[Size] = []() -> const type& {
      static const type INSTANCE;
      return INSTANCE;
    };
  }

  using func = const type&(*)();

  func cache_[Size+1];
  detail::initializer<Builder, Size> init_;
};

} // irstd
} // ROOT

#endif
