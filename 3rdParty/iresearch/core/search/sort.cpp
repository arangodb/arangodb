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

DEFINE_ATTRIBUTE_TYPE(iresearch::boost);
DEFINE_FACTORY_DEFAULT(boost);

boost::boost()
  : basic_stored_attribute<boost::boost_t>(boost_t(boost::no_boost())) {
}

// ----------------------------------------------------------------------------
// --SECTION--                                                             sort
// ----------------------------------------------------------------------------

sort::sort(const type_id& type) : type_(&type) { }

sort::~sort() { }

sort::collector::~collector() { }

sort::scorer::~scorer() { }

sort::prepared::prepared(attribute_view&& attrs): attrs_(std::move(attrs)) {
}

sort::prepared::~prepared() { }

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
    pord.order_.emplace_back(entry.sort().prepare(), entry.reverse());
    prepared::prepared_sort& psort = pord.order_.back();
    const sort::prepared& bucket = *psort.bucket;
    pord.features_ |= bucket.features();
    psort.offset = offset;
    pord.size_ += bucket.size();
    offset += bucket.size();
  }

  return pord;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                            stats 
// ----------------------------------------------------------------------------
order::prepared::stats::stats(collectors_t&& colls)
  : colls_(std::move(colls)) {
}

order::prepared::stats::stats(stats&& rhs) NOEXCEPT
  : colls_(std::move(rhs.colls_)) {
}

order::prepared::stats& order::prepared::stats::operator=(
  stats&& rhs
) NOEXCEPT {
  if (this != &rhs) {
    colls_ = std::move(rhs.colls_);
  }

  return *this;
}

void order::prepared::stats::collect(
    const sub_reader& segment,
    const term_reader& field,
    const attribute_view& term_attrs
) const {
  for (auto& collector : colls_) {
    collector->collect(segment, field, term_attrs);
  }
}

void order::prepared::stats::finish(
    attribute_store& filter_attrs,
    const index_reader& index
) const {
  for (auto& collector : colls_) {
    collector->finish(filter_attrs, index);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                                          scorers
// ----------------------------------------------------------------------------

order::prepared::scorers::scorers(scorers_t&& scorers)
  : scorers_(std::move(scorers)) {
}

order::prepared::scorers::scorers(order::prepared::scorers&& rhs) NOEXCEPT
  : scorers_(std::move(rhs.scorers_)) {
}

order::prepared::scorers& order::prepared::scorers::operator=(
  order::prepared::scorers&& rhs
) NOEXCEPT {
  if (this != &rhs) {
    scorers_ = std::move(rhs.scorers_);
  }

  return *this;
}

void order::prepared::scorers::score(
  const order::prepared& ord, byte_type* scr
) const {
  size_t i = 0;
  std::for_each(
    scorers_.begin(), scorers_.end(),
    [&ord, &scr, &i] (const sort::scorer::ptr& scorer) {
      if (scorer) scorer->score(scr);
      const sort::prepared& bucket = *ord[i++].bucket;
      scr += bucket.size();
  });
}

order::prepared::prepared() : size_(0) { }

order::prepared::stats 
order::prepared::prepare_stats() const {
  /* initialize collectors */
  stats::collectors_t colls;
  colls.reserve(size());
  for_each([&colls](const prepared_sort& ps) {
    sort::collector::ptr collector = ps.bucket->prepare_collector();
    if (collector) {
      colls.emplace_back(std::move(collector));
    }
  });
  return prepared::stats(std::move(colls));
}

void order::prepared::prepare_score(byte_type* score) const {
  for (auto& sort : order_) {
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
  scorers::scorers_t scrs;
  scrs.reserve(size());
  for_each([&segment, &field, &stats, &doc, &scrs] (const order::prepared::prepared_sort& ps) {
    const sort::prepared& bucket = *ps.bucket;
    scrs.emplace_back(bucket.prepare_scorer(segment, field, stats, doc));
  });
  return prepared::scorers(std::move(scrs));
}

bool order::prepared::less(const byte_type* lhs, const byte_type* rhs) const {
  if (!lhs) {
    return rhs != nullptr; // lhs(nullptr) == rhs(nullptr)
  }

  if (!rhs) {
    return true; // nullptr last
  }

  for (auto& prepared_sort: order_) {
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
