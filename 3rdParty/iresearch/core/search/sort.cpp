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

#include "sort.hpp"

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "utils/memory_pool.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                             sort
// ----------------------------------------------------------------------------

sort::sort(const type_id& type) NOEXCEPT
  : type_(&type) {
}

sort::prepared::prepared(attribute_view&& attrs) NOEXCEPT
  : attrs_(std::move(attrs)) {
}

// ----------------------------------------------------------------------------
// --SECTION--                                                            order 
// ----------------------------------------------------------------------------

const order& order::unordered() {
  static order ord;
  return ord;
}

void order::remove(const type_id& type) {
  order_.erase(
    std::remove_if(
      order_.begin(), order_.end(),
      [type] (const entry& e) { return type == e.sort().type(); }
  ));
}

const order::prepared& order::prepared::unordered() {
  static order::prepared ord;
  return ord;
}

order::prepared::prepared(order::prepared&& rhs) NOEXCEPT
  : order_(std::move(rhs.order_)),
    features_(std::move(rhs.features_)),
    score_size_(rhs.score_size_),
    stats_size_(rhs.stats_size_) {
  rhs.score_size_ = 0;
  rhs.stats_size_ = 0;
}

order::prepared& order::prepared::operator=(order::prepared&& rhs) NOEXCEPT {
  if (this != &rhs) {
    order_ = std::move(rhs.order_);
    features_ = std::move(rhs.features_);
    score_size_ = rhs.score_size_;
    rhs.score_size_ = 0;
    stats_size_ = rhs.stats_size_;
    rhs.stats_size_ = 0;
  }

  return *this;
}

bool order::operator==(const order& other) const {
  if (order_.size() != other.order_.size()) {
    return false;
  }

  for (size_t i = 0, count = order_.size(); i < count; ++i) {
    auto& entry = order_[i];
    auto& other_entry = other.order_[i];

    auto& sort = entry.sort_;
    auto& other_sort = other_entry.sort_;

    // FIXME TODO operator==(...) should be specialized for every sort child class based on init config
    if (!sort != !other_sort
        || (sort
            && (sort->type() != other_sort->type()
                || entry.reverse_ != other_entry.reverse_))) {
      return false;
    }
  }

  return true;
}

order& order::add(bool reverse, const sort::ptr& sort) {
  assert(sort);
  order_.emplace_back(sort, reverse);

  return *this;
}

order::prepared order::prepare() const {
  order::prepared pord;
  pord.order_.reserve(order_.size());

  size_t stats_align = 0;
  size_t score_align = 0;

  for (auto& entry : order_) {
    auto prepared = entry.sort().prepare();

    if (!prepared) {
      // skip empty sorts
      continue;
    }

    const auto score_size = prepared->score_size();
    assert(score_size.second <= ALIGNOF(MAX_ALIGN_T));
    assert(score_size.second && math::is_power2(score_size.second));

    const auto stats_size = prepared->stats_size();
    assert(stats_size.second <= ALIGNOF(MAX_ALIGN_T));
    assert(stats_size.second && math::is_power2(stats_size.second));

    stats_align = std::max(stats_align, stats_size.second);
    score_align = std::max(score_align, score_size.second);

    pord.score_size_ = memory::align_up(pord.score_size_, score_size.second);
    pord.stats_size_ = memory::align_up(pord.stats_size_, stats_size.second);
    pord.features_ |= prepared->features();

    pord.order_.emplace_back(
      std::move(prepared),
      pord.score_size_,
      pord.stats_size_,
      entry.reverse()
    );

    pord.score_size_ += memory::align_up(score_size.first, score_size.second);
    pord.stats_size_ += memory::align_up(stats_size.first, stats_size.second);
  }

  pord.stats_size_ = memory::align_up(pord.stats_size_, stats_align);
  pord.score_size_ = memory::align_up(pord.score_size_, score_align);

  return pord;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        collectors
// -----------------------------------------------------------------------------

order::prepared::collectors::collectors(
    const order::prepared& buckets,
    size_t terms_count
): buckets_(buckets.order_) {
   field_collectors_.reserve(buckets_.size());
   term_collectors_.reserve(buckets_.size() * terms_count);

  // add field collectors from each bucket
  for (auto& entry: buckets_) {
    assert(entry.bucket); // ensured by order::prepare
    field_collectors_.emplace_back(entry.bucket->prepare_field_collector());
  }

  // add term collectors from each bucket
  // layout order [t0.b0, t0.b1, ... t0.bN, t1.b0, t1.b1 ... tM.BN]
  for (size_t i = 0; i < terms_count; ++i) {
    for (auto& entry: buckets_) {
      assert(entry.bucket); // ensured by order::prepare
      term_collectors_.emplace_back(entry.bucket->prepare_term_collector());
    }
  }
}

order::prepared::collectors::collectors(collectors&& other) NOEXCEPT
  : buckets_(other.buckets_),
    field_collectors_(std::move(other.field_collectors_)),
    term_collectors_(std::move(other.term_collectors_)) {
}

void order::prepared::collectors::collect(
  const sub_reader& segment,
  const term_reader& field
) const {
  for (auto& entry: field_collectors_) {
    if (entry) { // may be null if prepare_field_collector() returned nullptr
      entry->collect(segment, field);
    }
  }
}

void order::prepared::collectors::collect(
  const sub_reader& segment,
  const term_reader& field,
  size_t term_offset,
  const attribute_view& term_attrs
) const {
  for (size_t i = 0, count = buckets_.size(); i < count; ++i) {
    assert(i * buckets_.size() + term_offset < term_collectors_.size()); // enforced by allocation in the constructor
    auto& entry = term_collectors_[term_offset * buckets_.size() + i];

    if (entry) { // may be null if prepare_term_collector() returned nullptr
      entry->collect(segment, field, term_attrs);
    }
  }
}

void order::prepared::collectors::finish(
    byte_type* stats_buf,
    const index_reader& index) const {
  // special case where term statistics collection is not applicable
  // e.g. by_column_existence filter
  if (term_collectors_.empty()) {
    assert(field_collectors_.size() == buckets_.size()); // enforced by allocation in the constructor

    for (size_t i = 0, count = field_collectors_.size(); i < count; ++i) {
      auto& sort = buckets_[i];
      assert(sort.bucket); // ensured by order::prepare

      sort.bucket->collect(
        stats_buf + sort.stats_offset, // where stats for bucket start
        index,
        field_collectors_[i].get(),
        nullptr
      );
    }
  } else {
    auto bucket_count = buckets_.size();
    assert(term_collectors_.size() % bucket_count == 0); // enforced by allocation in the constructor

    for (size_t i = 0, count = term_collectors_.size(); i < count; ++i) {
      auto bucket_offset = i % bucket_count;
      auto& sort = buckets_[bucket_offset];
      assert(sort.bucket); // ensured by order::prepare

      assert(i % bucket_count < field_collectors_.size()); // enforced by allocation in the constructor
      sort.bucket->collect(
        stats_buf + sort.stats_offset, // where stats for bucket start
        index,
        field_collectors_[bucket_offset].get(),
        term_collectors_[i].get()
      );
    }
  }
}

size_t order::prepared::collectors::push_back() {
  auto term_offset = term_collectors_.size() / buckets_.size();

  term_collectors_.reserve(term_collectors_.size() + buckets_.size());

  for (auto& entry: buckets_) {
    assert(entry.bucket); // ensured by order::prepare
    term_collectors_.emplace_back(entry.bucket->prepare_term_collector());
  }

  return term_offset;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                          scorers
// ----------------------------------------------------------------------------

order::prepared::scorers::scorers(
    const prepared_order_t& buckets,
    const sub_reader& segment,
    const term_reader& field,
    const byte_type* stats_buf,
    const attribute_view& doc,
    boost_t boost
) {
  scorers_.reserve(buckets.size());

  for (auto& entry: buckets) {
    assert(stats_buf);
    assert(entry.bucket); // ensured by order::prepared
    const auto& bucket = *entry.bucket;

    auto scorer = bucket.prepare_scorer(
      segment, field, stats_buf + entry.stats_offset, doc, boost
    );

    if (scorer) {
      // skip empty scorers
      scorers_.emplace_back(std::move(scorer), entry.score_offset);
    }
  }
}

order::prepared::scorers::scorers(order::prepared::scorers&& other) NOEXCEPT
  : scorers_(std::move(other.scorers_)) {
}

order::prepared::scorers& order::prepared::scorers::operator=(
    order::prepared::scorers&& other
) NOEXCEPT {
  if (this != &other) {
    scorers_ = std::move(other.scorers_);
  }

  return *this;
}

void order::prepared::scorers::score(byte_type* scr) const {
  for (auto& scorer : scorers_) {
    assert(scorer.first);
    scorer.first->score(scr + scorer.second);
  }
}

void order::prepared::prepare_collectors(
    byte_type* stats_buf,
    const index_reader& index
) const {
  for (auto& entry: order_) {
    assert(entry.bucket); // ensured by order::prepared
    entry.bucket->collect(stats_buf + entry.stats_offset, index, nullptr, nullptr);
  }
}

void order::prepared::prepare_score(byte_type* score) const {
  for (auto& sort : order_) {
    assert(sort.bucket);
    sort.bucket->prepare_score(score + sort.score_offset);
  }
}

void order::prepared::prepare_stats(byte_type* stats) const {
  for (auto& sort : order_) {
    assert(sort.bucket);
    sort.bucket->prepare_score(stats + sort.stats_offset);
  }
}

bool order::prepared::less(const byte_type* lhs, const byte_type* rhs) const {
  if (!lhs) {
    return rhs != nullptr; // lhs(nullptr) == rhs(nullptr)
  }

  if (!rhs) {
    return true; // nullptr last
  }

  for (const auto& sort: order_) {
    assert(sort.bucket); // ensured by order::prepared
    const auto& bucket = *sort.bucket;
    const auto* lhs_begin = lhs + sort.score_offset;
    const auto* rhs_begin = rhs + sort.score_offset;

    if (bucket.less(lhs_begin, rhs_begin)) {
      return !sort.reverse;
    }

    if (bucket.less(rhs_begin, lhs_begin)) {
      return sort.reverse;
    }
  }

  return false;
}

void order::prepared::add(byte_type* lhs, const byte_type* rhs) const {
  for_each([lhs, rhs] (const prepared_sort& sort) {
    assert(sort.bucket);
    sort.bucket->add(lhs + sort.score_offset, rhs + sort.score_offset);
  });
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
