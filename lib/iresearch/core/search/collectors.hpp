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

#pragma once

#include <vector>

#include "search/scorer.hpp"
#include "shared.hpp"

namespace irs {

// A convinience base class for collector wrappers
template<typename Wrapper, typename Collector>
class collector_wrapper {
 public:
  using collector_ptr = typename Collector::ptr;
  using element_type = typename collector_ptr::element_type;
  using pointer = typename collector_ptr::pointer;

  pointer get() const noexcept { return collector_.get(); }
  pointer operator->() const noexcept { return get(); }
  element_type& operator*() const noexcept { return *collector_; }
  explicit operator bool() const noexcept {
    return static_cast<bool>(collector_);
  }

 protected:
  collector_wrapper() noexcept : collector_(&Wrapper::noop()) {}

  ~collector_wrapper() { reset(nullptr); }

  explicit collector_wrapper(pointer collector) noexcept
    : collector_(!collector ? &Wrapper::noop() : collector) {
    IRS_ASSERT(collector_);
  }

  collector_wrapper(collector_wrapper&& rhs) noexcept
    : collector_(std::move(rhs.collector_)) {
    rhs.collector_.reset(&Wrapper::noop());
    IRS_ASSERT(collector_);
  }

  collector_wrapper& operator=(pointer collector) noexcept {
    if (!collector) {
      collector = &Wrapper::noop();
    }

    if (collector_.get() != collector) {
      reset(collector);
    }

    IRS_ASSERT(collector_);
    return *this;
  }

  collector_wrapper& operator=(collector_wrapper&& rhs) noexcept {
    if (this != &rhs) {
      reset(rhs.collector_.release());
      rhs.collector_.reset(&Wrapper::noop());
    }
    IRS_ASSERT(collector_);
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

// A convinience base class for collectors
template<typename Collector>
class collectors_base {
 public:
  using iterator_type = typename std::vector<Collector>::const_iterator;

  explicit collectors_base(size_t size, const Scorers& order)
    : collectors_(size), buckets_{order.buckets()} {}

  collectors_base(collectors_base&&) = default;
  collectors_base& operator=(collectors_base&&) = default;

  iterator_type begin() const noexcept { return collectors_.begin(); }

  iterator_type end() const noexcept { return collectors_.end(); }

  bool empty() const noexcept { return collectors_.empty(); }

  void reset() {
    for (auto& collector : collectors_) {
      collector->reset();
    }
  }

  typename Collector::pointer front() const noexcept {
    IRS_ASSERT(!collectors_.empty());
    return collectors_.front().get();
  }

  typename Collector::pointer back() const noexcept {
    IRS_ASSERT(!collectors_.empty());
    return collectors_.back().get();
  }

  typename Collector::pointer operator[](size_t i) const noexcept {
    IRS_ASSERT(i < collectors_.size());
    return collectors_[i].get();
  }

 protected:
  std::vector<Collector> collectors_;
  std::span<const ScorerBucket> buckets_;
};

// Wrapper around FieldCollector which guarantees collector
// is not nullptr
class field_collector_wrapper
  : public collector_wrapper<field_collector_wrapper, FieldCollector> {
 public:
  using collector_type = FieldCollector;
  using base_type = collector_wrapper<field_collector_wrapper, collector_type>;

  static collector_type& noop() noexcept;

  field_collector_wrapper() = default;
  field_collector_wrapper(field_collector_wrapper&&) = default;
  field_collector_wrapper& operator=(field_collector_wrapper&&) = default;
  explicit field_collector_wrapper(collector_type::ptr&& collector) noexcept
    : base_type(collector.release()) {}
  field_collector_wrapper& operator=(collector_type::ptr&& collector) noexcept {
    base_type::operator=(collector.release());
    return *this;
  }
};

static_assert(std::is_nothrow_move_constructible_v<field_collector_wrapper>);
static_assert(std::is_nothrow_move_assignable_v<field_collector_wrapper>);

// Create an field level index statistics compound collector for
// all buckets
class field_collectors : public collectors_base<field_collector_wrapper> {
 public:
  explicit field_collectors(const Scorers& buckets);
  field_collectors(field_collectors&&) = default;
  field_collectors& operator=(field_collectors&&) = default;

  size_t size() const noexcept { return collectors_.size(); }

  // Collect field related statistics, i.e. field used in the filter
  // segment the segment being processed (e.g. for columnstore)
  // field the field matched by the filter in the 'segment'
  // Note called once for every field matched by a filter per each segment
  // Note always called on each matched 'field' irrespective of if it
  // contains a matching 'term'
  void collect(const SubReader& segment, const term_reader& field) const;

  // Store collected index statistics into 'stats' of the
  // current 'filter'
  // stats out-parameter to store statistics for later use in
  // calls to score(...)
  // Note called once on the 'index' for every term matched by a filter
  //       calling collect(...) on each of its segments
  // Note if not matched terms then called exactly once
  void finish(byte_type* stats_buf) const;
};

static_assert(std::is_nothrow_move_constructible_v<field_collectors>);
static_assert(std::is_nothrow_move_assignable_v<field_collectors>);

// Wrapper around TermCollector which guarantees collector
// is not nullptr
class term_collector_wrapper
  : public collector_wrapper<term_collector_wrapper, TermCollector> {
 public:
  using collector_type = TermCollector;
  using base_type = collector_wrapper<term_collector_wrapper, collector_type>;

  static collector_type& noop() noexcept;

  term_collector_wrapper() = default;
  term_collector_wrapper(term_collector_wrapper&&) = default;
  term_collector_wrapper& operator=(term_collector_wrapper&&) = default;
  explicit term_collector_wrapper(collector_type::ptr&& collector) noexcept
    : base_type(collector.release()) {}
  term_collector_wrapper& operator=(collector_type::ptr&& collector) noexcept {
    base_type::operator=(collector.release());
    return *this;
  }
};

static_assert(std::is_nothrow_move_constructible_v<term_collector_wrapper>);
static_assert(std::is_nothrow_move_assignable_v<term_collector_wrapper>);

// Create an term level index statistics compound collector for
// all buckets
class term_collectors : public collectors_base<term_collector_wrapper> {
 public:
  term_collectors(const Scorers& buckets, size_t size);
  term_collectors(term_collectors&&) = default;
  term_collectors& operator=(term_collectors&&) = default;

  size_t size() const noexcept {
    return buckets_.size() ? collectors_.size() / buckets_.size() : 0;
  }

  // Add collectors for another term and return term_offset
  size_t push_back();

  // Collect term related statistics, i.e. term used in the filter
  // segment the segment being processed (e.g. for columnstore)
  // field the field matched by the filter in the 'segment'
  // term_index index of term, value < constructor 'terms_count'
  // term_attributes the attributes of the matched term in the field
  // Note called once for every term matched by a filter in the 'field'
  //       per each segment
  // Note only called on a matched 'term' in the 'field' in the 'segment'
  void collect(const SubReader& segment, const term_reader& field,
               size_t term_idx, const attribute_provider& attrs) const;

  // Store collected index statistics into 'stats' of the
  // current 'filter'
  // stats - out-parameter to store statistics for later use in
  // calls to score(...)
  // term_index - index of term, value < constructor 'terms_count'
  // index - the full index to collect statistics on
  // Note called once on the 'index' for every term matched by a filter
  //       calling collect(...) on each of its segments
  // Note if not matched terms then called exactly once
  void finish(byte_type* stats_buf, size_t term_idx,
              const field_collectors& field_collectors,
              const IndexReader& index) const;
};

static_assert(std::is_nothrow_move_constructible_v<term_collectors>);
static_assert(std::is_nothrow_move_assignable_v<term_collectors>);

}  // namespace irs
