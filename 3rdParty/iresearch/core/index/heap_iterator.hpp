////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_PQ_ITERATOR_H
#define IRESEARCH_PQ_ITERATOR_H

#include "shared.hpp"
#include "iterators.hpp"
#include "utils/ebo.hpp"

#include <algorithm>
#include <vector>

NS_ROOT

template<typename Iterator>
struct iterator_traits {
  static bool next(Iterator&);
}; // iterator_traits

template<>
struct iterator_traits<doc_iterator::ptr> {
  static bool next(doc_iterator::ptr& it) {
    return it->next();
  }
};

template<
  typename Iterator,
  typename Less,
  typename IteratorTraits = iterator_traits<Iterator>
> class heap_iterator : public doc_iterator,
                        private compact<0, Less> {
 private:
  typedef compact<0, Less> less_store_t;

 public:
  typedef Iterator iterator_t;
  typedef Less less_t;
  typedef IteratorTraits traits_t;

 public:
  explicit heap_iterator(less_t less = {})
    : less_store_t(less) {
  }

  template<typename... Args>
  void emplace_back(Args... args) {
    heap_.emplace_back(std::forward<Args>(args)...);
  }

  bool next() {
    while (lead_) {
      auto begin = std::begin(heap_);
      auto it = std::end(heap_) - lead_--;

      if (!IteratorTraits::next(it->first)) {
        if (!remove_lead(it)) {
          return false;
        }

        continue;
      }

      std::push_heap(begin, ++it, less());
    }

    assert(!heap_.empty());
    std::pop_heap(std::begin(heap_), std::end(heap_), less());

    lead_ = 1;

    return true;
  }

 protected:
  const Iterator& lead() const NOEXCEPT {
    assert(!heap_.empty());
    return heap_.back();
  }

 private:
  bool remove_lead(Iterator it) {
    if (&*it != &heap_.back()) {
      std::swap(*it, heap_.back());
    }
    heap_.pop_back();
    return !heap_.empty();
  }

  const less_t& less() const NOEXCEPT {
    return less_store_t::get();
  }

  std::vector<Iterator> heap_;
  size_t lead_;
}; // heap_iterator

////////////////////////////////////////////////////////////////////////////////
/// @class external_heap_iterator
///-----------------------------------------------------------------------------
///      [0] <-- begin
///      [1]      |
///      [2]      | head (heap)
///      [3]      |
///      [4] <-- lead_
///      [5]      |
///      [6]      | lead (list of accepted iterators)
///      ...      |
///      [n] <-- end
///-----------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////
template<typename Context>
class external_heap_iterator : private compact<0, Context> {
 private:
  typedef compact<0, Context> context_store_t;

 public:
  typedef Context context_t;

  explicit external_heap_iterator(context_t ctx = {})
    : context_store_t(ctx) {
  }

  void reset(size_t size = 0) {
    heap_.resize(size);
    std::iota(heap_.begin(), heap_.end(), size_t(0));
    lead_ = size;
  }

  bool next() {
    if (heap_.empty()) {
      return false;
    }

    auto begin = std::begin(heap_);

    while (lead_) {
      auto it = std::end(heap_) - lead_--;

      if (!context()(*it)) { // advance iterator
        if (!remove_lead(it)) {
          assert(heap_.empty());
          return false;
        }

        continue;
      }

      std::push_heap(begin, ++it, context());
    }

    assert(!heap_.empty());
    std::pop_heap(begin, std::end(heap_), context());
    lead_ = 1;

    return true;
  }

  size_t value() const NOEXCEPT {
    assert(!heap_.empty());
    return heap_.back();
  }

 private:
  bool remove_lead(std::vector<size_t>::iterator it) {
    if (&*it != &heap_.back()) {
      std::swap(*it, heap_.back());
    }
    heap_.pop_back();
    return !heap_.empty();
  }

  const context_t& context() const NOEXCEPT {
    return context_store_t::get();
  }

  std::vector<size_t> heap_;
  size_t lead_{};
}; // external_heap_iterator

NS_END // ROOT

#endif // IRESEARCH_PQ_ITERATOR_H


