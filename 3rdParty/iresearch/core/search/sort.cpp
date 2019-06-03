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

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                       Attributes
// ----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(iresearch::boost)
DEFINE_FACTORY_DEFAULT(boost)

boost::boost() NOEXCEPT
  : basic_stored_attribute<boost::boost_t>(boost_t(boost::no_boost())) {
}

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
    size_(rhs.size_) {
  rhs.size_ = 0;
}

order::prepared& order::prepared::operator=(order::prepared&& rhs) NOEXCEPT {
  if (this != &rhs) {
    order_ = std::move(rhs.order_);
    features_ = std::move(rhs.features_);
    size_ = rhs.size_;
    rhs.size_ = 0;
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

bool order::operator!=(const order& other) const {
  return !(*this == other);
}

order& order::add(bool reverse, const sort::ptr& sort) {
  assert(sort);
  order_.emplace_back(sort, reverse);

  return *this;
}

order::prepared order::prepare() const {
  order::prepared pord;
  pord.order_.reserve(order_.size()); // strong exception guarantee

  size_t offset = 0;
  for (auto& entry : order_) {
    auto prepared = entry.sort().prepare();

    if (!prepared) {
      // skip empty sorts
      continue;
    }

    pord.order_.emplace_back(std::move(prepared), entry.reverse());

    prepared::prepared_sort& psort = pord.order_.back();
    const sort::prepared& bucket = *psort.bucket;
    pord.features_ |= bucket.features();
    psort.offset = offset;
    pord.size_ += bucket.size();
    offset += bucket.size();
  }

  return pord;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        collectors
// -----------------------------------------------------------------------------

order::prepared::collectors::collectors(
    const prepared_order_t& buckets,
    size_t terms_count
): buckets_(buckets) {
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
  attribute_store& filter_attrs,
  const index_reader& index
) const {
  // special case where term statistics collection is not applicable
  // e.g. by_column_existence filter
  if (term_collectors_.empty()) {
    assert(field_collectors_.size() == buckets_.size()); // enforced by allocation in the constructor

    for (size_t i = 0, count = field_collectors_.size(); i < count; ++i) {
      auto& bucket = buckets_[i].bucket;
      assert(bucket); // ensured by order::prepare

      bucket->collect(filter_attrs, index, field_collectors_[i].get(), nullptr);
    }
  } else {
    auto bucket_count = buckets_.size();
    assert(term_collectors_.size() % bucket_count == 0); // enforced by allocation in the constructor

    for (size_t i = 0, count = term_collectors_.size(); i < count; ++i) {
      auto bucket_offset = i % bucket_count;
      auto& bucket = buckets_[bucket_offset].bucket;
      assert(bucket); // ensured by order::prepare

      assert(i % bucket_count < field_collectors_.size()); // enforced by allocation in the constructor
      bucket->collect(
        filter_attrs,
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
    const attribute_store& stats,
    const attribute_view& doc
) {
  scorers_.reserve(buckets.size());

  for (auto& entry: buckets) {
    assert(entry.bucket); // ensured by order::prepared
    const auto& bucket = *entry.bucket;

    auto scorer = bucket.prepare_scorer(segment, field, stats, doc);

    if (scorer) {
      // skip empty scorers
      scorers_.emplace_back(std::move(scorer), entry.offset);
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

order::prepared::prepared() : size_(0) { }

order::prepared::collectors order::prepared::prepare_collectors(
    size_t terms_count /*= 0*/
) const {
  return collectors(order_, terms_count);
}

void order::prepared::prepare_collectors(
    attribute_store& filter_attrs,
    const index_reader& index
) const {
  for (auto& entry: order_) {
    assert(entry.bucket); // ensured by order::prepared
    entry.bucket->collect(filter_attrs, index, nullptr, nullptr);
  }
}

void order::prepared::prepare_score(byte_type* score) const {
  for (auto& sort : order_) {
    assert(sort.bucket);
    sort.bucket->prepare_score(score + sort.offset);
  }
}

order::prepared::scorers 
order::prepared::prepare_scorers(
    const sub_reader& segment,
    const term_reader& field,
    const attribute_store& stats,
    const attribute_view& doc
) const {
  return scorers(order_, segment, field, stats, doc);
}

bool order::prepared::less(const byte_type* lhs, const byte_type* rhs) const {
  if (!lhs) {
    return rhs != nullptr; // lhs(nullptr) == rhs(nullptr)
  }

  if (!rhs) {
    return true; // nullptr last
  }

  for (auto& prepared_sort: order_) {
    assert(prepared_sort.bucket); // ensured by order::prepared
    auto& bucket = *(prepared_sort.bucket);

    if (bucket.less(lhs, rhs)) {
      return !prepared_sort.reverse;
    }

    if (bucket.less(rhs, lhs)) {
      return prepared_sort.reverse;
    }

    lhs += bucket.size();
    rhs += bucket.size();
  }

  return false;
}

void order::prepared::add(byte_type* lhs, const byte_type* rhs) const {
  for_each([&lhs, &rhs] (const prepared_sort& ps) {
    const sort::prepared& bucket = *ps.bucket;
    bucket.add(lhs, rhs);
    lhs += bucket.size();
    rhs += bucket.size();
  });
}

order::order(order&& rhs) NOEXCEPT
  : order_(std::move(rhs.order_)) {
}

order& order::operator=(order&& rhs) NOEXCEPT {
  if (this != &rhs) {
    order_ = std::move(rhs.order_);
  }

  return *this;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
