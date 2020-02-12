// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ITERATOR_H_
#define V8_BASE_ITERATOR_H_

#include <iterator>

namespace v8 {
namespace base {

template <class Category, class Type, class Diff = std::ptrdiff_t,
          class Pointer = Type*, class Reference = Type&>
struct iterator {
  using iterator_category = Category;
  using value_type = Type;
  using difference_type = Diff;
  using pointer = Pointer;
  using reference = Reference;
};

// The intention of the base::iterator_range class is to encapsulate two
// iterators so that the range defined by the iterators can be used like
// a regular STL container (actually only a subset of the full container
// functionality is available usually).
template <typename ForwardIterator>
class iterator_range {
 public:
  using iterator = ForwardIterator;
  using const_iterator = ForwardIterator;
  using pointer = typename std::iterator_traits<iterator>::pointer;
  using reference = typename std::iterator_traits<iterator>::reference;
  using value_type = typename std::iterator_traits<iterator>::value_type;
  using difference_type =
      typename std::iterator_traits<iterator>::difference_type;

  iterator_range() : begin_(), end_() {}
  template <typename ForwardIterator1, typename ForwardIterator2>
  iterator_range(ForwardIterator1&& begin, ForwardIterator2&& end)
      : begin_(std::forward<ForwardIterator1>(begin)),
        end_(std::forward<ForwardIterator2>(end)) {}

  iterator begin() { return begin_; }
  iterator end() { return end_; }
  const_iterator begin() const { return begin_; }
  const_iterator end() const { return end_; }
  const_iterator cbegin() const { return begin_; }
  const_iterator cend() const { return end_; }

  bool empty() const { return cbegin() == cend(); }

  // Random Access iterators only.
  reference operator[](difference_type n) { return begin()[n]; }
  difference_type size() const { return cend() - cbegin(); }

 private:
  const_iterator const begin_;
  const_iterator const end_;
};

template <typename ForwardIterator>
auto make_iterator_range(ForwardIterator&& begin, ForwardIterator&& end) {
  return iterator_range<ForwardIterator>{std::forward<ForwardIterator>(begin),
                                         std::forward<ForwardIterator>(end)};
}

// {Reversed} returns a container adapter usable in a range-based "for"
// statement for iterating a reversible container in reverse order.
//
// Example:
//
//   std::vector<int> v = ...;
//   for (int i : base::Reversed(v)) {
//     // iterates through v from back to front
//   }
template <typename T>
auto Reversed(T& t) {  // NOLINT(runtime/references): match {rbegin} and {rend}
  return make_iterator_range(std::rbegin(t), std::rend(t));
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ITERATOR_H_
