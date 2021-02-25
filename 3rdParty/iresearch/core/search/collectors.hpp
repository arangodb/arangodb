////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COLLECTORS_H
#define IRESEARCH_COLLECTORS_H

#include <vector>

#include "shared.hpp"
#include "sort.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////
/// @class collector_wrapper
/// @brief a convinience base class for collector wrappers
////////////////////////////////////////////////////////////////////////////
template<typename Wrapper, typename Collector>
class collector_wrapper {
 public:
  using collector_ptr = typename Collector::ptr;
  using element_type = typename collector_ptr::element_type;
  using pointer = typename collector_ptr::pointer;

  pointer get() const noexcept { return collector_.get(); }
  pointer operator->() const noexcept { return get(); }
  element_type& operator*() const noexcept { return *collector_; }
  explicit operator bool() const noexcept { return static_cast<bool>(collector_); }

 protected:
  collector_wrapper() noexcept
    : collector_(&Wrapper::noop()) {
  }

  ~collector_wrapper() {
    reset(nullptr);
  }

  explicit collector_wrapper(pointer collector) noexcept
    : collector_(!collector ? &Wrapper::noop() : collector) {
    assert(collector_);
  }

  collector_wrapper(collector_wrapper&& rhs) noexcept
    : collector_(std::move(rhs.collector_)) {
    rhs.collector_.reset(&Wrapper::noop());
    assert(collector_);
  }

  collector_wrapper& operator=(pointer collector) noexcept {
    if (!collector) {
      collector = &Wrapper::noop();
    }

    if (collector_.get() != collector) {
      reset(collector);
    }

    assert(collector_);
    return *this;
  }

  collector_wrapper& operator=(collector_wrapper&& rhs) noexcept {
    if (this != &rhs) {
      reset(rhs.collector_.release());
      rhs.collector_.reset(&Wrapper::noop());
    }
    assert(collector_);
    return *this;
  }

 private:
  void reset(pointer collector) noexcept {
    if (collector_.get() == &Wrapper::noop()) {
      collector_.release();
    }
    collector_.reset(collector);
  }

  collector_ptr collector_;
};

////////////////////////////////////////////////////////////////////////////
/// @class collectors_base
/// @brief a convinience base class for collectors
////////////////////////////////////////////////////////////////////////////
template<typename Collector>
class collectors_base {
 public:
  using iterator_type = typename std::vector<Collector>::const_iterator;

  explicit collectors_base(size_t size, const order::prepared& buckets)
    : collectors_(size), buckets_(&buckets) {
  }

  collectors_base(collectors_base&&) = default;
  collectors_base& operator=(collectors_base&&) = default;

  iterator_type begin() const noexcept {
    return collectors_.begin();
  }

  iterator_type end() const noexcept {
    return collectors_.end();
  }

  bool empty() const noexcept {
    return collectors_.empty();
  }

  void reset() {
    for (auto& collector : collectors_) {
      collector->reset();
    }
  }

  typename Collector::pointer front() const noexcept {
    assert(!collectors_.empty());
    return collectors_.front().get();
  }

  typename Collector::pointer back() const noexcept {
    assert(!collectors_.empty());
    return collectors_.back().get();
  }

  typename Collector::pointer operator[](size_t i) const noexcept {
    assert(i < collectors_.size());
    return collectors_[i].get();
  }

 protected:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::vector<Collector> collectors_;
  const order::prepared* buckets_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // collectors_base

////////////////////////////////////////////////////////////////////////////
/// @class field_collector_wrapper
/// @brief wrapper around sort::field_collector which guarantees collector
///        is not nullptr
////////////////////////////////////////////////////////////////////////////
class field_collector_wrapper
    : public collector_wrapper<field_collector_wrapper, sort::field_collector> {
 public:
  using collector_type = sort::field_collector;
  using base_type = collector_wrapper<field_collector_wrapper, collector_type>;

  IRESEARCH_API static collector_type& noop() noexcept;

  field_collector_wrapper() = default;
  field_collector_wrapper(field_collector_wrapper&&) = default;
  field_collector_wrapper& operator=(field_collector_wrapper&&) = default;
  field_collector_wrapper(collector_type::ptr&& collector) noexcept
    : base_type(collector.release()) {
  }
  field_collector_wrapper& operator=(collector_type::ptr&& collector) noexcept {
    base_type::operator=(collector.release());
    return *this;
  }
}; // field_collector_wrapper

static_assert(std::is_nothrow_move_constructible_v<field_collector_wrapper>);
static_assert(std::is_nothrow_move_assignable_v<field_collector_wrapper>);

////////////////////////////////////////////////////////////////////////////
/// @class field_collectors
/// @brief create an field level index statistics compound collector for
///        all buckets
////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API field_collectors : public collectors_base<field_collector_wrapper> {
 public:
  explicit field_collectors(const order::prepared& buckets);
  field_collectors(field_collectors&&) = default;
  field_collectors& operator=(field_collectors&&) = default;

  size_t size() const noexcept {
    return collectors_.size();
  }

  //////////////////////////////////////////////////////////////////////////
  /// @brief collect field related statistics, i.e. field used in the filter
  /// @param segment the segment being processed (e.g. for columnstore)
  /// @param field the field matched by the filter in the 'segment'
  /// @note called once for every field matched by a filter per each segment
  /// @note always called on each matched 'field' irrespective of if it
  ///       contains a matching 'term'
  //////////////////////////////////////////////////////////////////////////
  void collect(const sub_reader& segment, const term_reader& field) const;

  //////////////////////////////////////////////////////////////////////////
  /// @brief store collected index statistics into 'stats' of the
  ///        current 'filter'
  /// @param stats out-parameter to store statistics for later use in
  ///        calls to score(...)
  /// @param index the full index to collect statistics on
  /// @note called once on the 'index' for every term matched by a filter
  ///       calling collect(...) on each of its segments
  /// @note if not matched terms then called exactly once
  //////////////////////////////////////////////////////////////////////////
  void finish(byte_type* stats_buf, const index_reader& index) const;
}; // field_collectors

static_assert(std::is_nothrow_move_constructible_v<field_collectors>);
static_assert(std::is_nothrow_move_assignable_v<field_collectors>);

////////////////////////////////////////////////////////////////////////////
/// @class term_collector_wrapper
/// @brief wrapper around sort::term_collector which guarantees collector
///         is not nullptr
////////////////////////////////////////////////////////////////////////////
class term_collector_wrapper
    : public collector_wrapper<term_collector_wrapper, sort::term_collector> {
 public:
  using collector_type = sort::term_collector;
  using base_type = collector_wrapper<term_collector_wrapper, collector_type>;

  IRESEARCH_API static collector_type& noop() noexcept;

  term_collector_wrapper() = default;
  term_collector_wrapper(term_collector_wrapper&&) = default;
  term_collector_wrapper& operator=(term_collector_wrapper&&) = default;
  term_collector_wrapper(collector_type::ptr&& collector) noexcept
    : base_type(collector.release()) {
  }
  term_collector_wrapper& operator=(collector_type::ptr&& collector) noexcept {
    base_type::operator=(collector.release());
    return *this;
  }
}; // term_collector_wrapper

static_assert(std::is_nothrow_move_constructible_v<term_collector_wrapper>);
static_assert(std::is_nothrow_move_assignable_v<term_collector_wrapper>);

////////////////////////////////////////////////////////////////////////////
/// @class term_collectors
/// @brief create an term level index statistics compound collector for
///        all buckets
/// @param terms_count number of term_collectors to allocate
////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API term_collectors : public collectors_base<term_collector_wrapper> {
 public:
  term_collectors(const order::prepared& buckets, size_t size);
  term_collectors(term_collectors&&) = default;
  term_collectors& operator=(term_collectors&&) = default;

  size_t size() const noexcept {
    return buckets_->size() ? collectors_.size() / buckets_->size() : 0;
  }

  //////////////////////////////////////////////////////////////////////////
  /// @brief add collectors for another term
  /// @return term_offset
  //////////////////////////////////////////////////////////////////////////
  size_t push_back();

  //////////////////////////////////////////////////////////////////////////
  /// @brief collect term related statistics, i.e. term used in the filter
  /// @param segment the segment being processed (e.g. for columnstore)
  /// @param field the field matched by the filter in the 'segment'
  /// @param term_index index of term, value < constructor 'terms_count'
  /// @param term_attributes the attributes of the matched term in the field
  /// @note called once for every term matched by a filter in the 'field'
  ///       per each segment
  /// @note only called on a matched 'term' in the 'field' in the 'segment'
  //////////////////////////////////////////////////////////////////////////
  void collect(const sub_reader& segment,
               const term_reader& field,
               size_t term_idx,
               const attribute_provider& attrs) const;

  //////////////////////////////////////////////////////////////////////////
  /// @brief store collected index statistics into 'stats' of the
  ///        current 'filter'
  /// @param stats out-parameter to store statistics for later use in
  ///        calls to score(...)
  /// @param term_index index of term, value < constructor 'terms_count'
  /// @param index the full index to collect statistics on
  /// @note called once on the 'index' for every term matched by a filter
  ///       calling collect(...) on each of its segments
  /// @note if not matched terms then called exactly once
  //////////////////////////////////////////////////////////////////////////
  void finish(byte_type* stats_buf,
              size_t term_idx,
              const field_collectors& field_collectors,
              const index_reader& index) const;
};

static_assert(std::is_nothrow_move_constructible_v<term_collectors>);
static_assert(std::is_nothrow_move_assignable_v<term_collectors>);

}

#endif // IRESEARCH_COLLECTORS_H
