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

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "utils/memory_pool.hpp"

namespace {

using namespace irs;

const order UNORDERED;
const order::prepared PREPARED_UNORDERED;

const byte_type* no_score(score_ctx* ctx) noexcept {
  return reinterpret_cast<byte_type*>(ctx);
}

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                      filter_boost
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(filter_boost);

// -----------------------------------------------------------------------------
// --SECTION--                                                    score_function
// -----------------------------------------------------------------------------

score_function::score_function() noexcept
  : func_(&::no_score) {
}

score_function::score_function(score_function&& rhs) noexcept
  : ctx_(std::move(rhs.ctx_)),
    func_(rhs.func_) {
  rhs.func_ = &::no_score;
}

score_function& score_function::operator=(score_function&& rhs) noexcept {
  if (this != &rhs) {
    ctx_ = std::move(rhs.ctx_);
    func_ = rhs.func_;
    rhs.func_ = &::no_score;
  }
  return *this;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              sort
// -----------------------------------------------------------------------------

sort::sort(const type_info& type) noexcept
  : type_(type.id()) {
}

// ----------------------------------------------------------------------------
// --SECTION--                                                            order 
// ----------------------------------------------------------------------------

const order& order::unordered() noexcept {
  return UNORDERED;
}

void order::remove(type_info::type_id type) {
  order_.erase(
    std::remove_if(
      order_.begin(), order_.end(),
      [type] (const entry& e) { return type == e.sort().type(); }
  ));
}

const order::prepared& order::prepared::unordered() noexcept {
  return PREPARED_UNORDERED;
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

order& order::add(bool reverse, sort::ptr&& sort) {
  assert(sort);
  order_.emplace_back(std::move(sort), reverse);

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
    assert(score_size.second <= alignof(std::max_align_t));
    assert(math::is_power2(score_size.second)); // math::is_power2(0) returns true

    const auto stats_size = prepared->stats_size();
    assert(stats_size.second <= alignof(std::max_align_t));
    assert(math::is_power2(stats_size.second)); // math::is_power2(0) returns true

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


// ----------------------------------------------------------------------------
// --SECTION--                                                          scorers
// ----------------------------------------------------------------------------

order::prepared::scorers::scorers(
    const order::prepared& buckets,
    const sub_reader& segment,
    const term_reader& field,
    const byte_type* stats_buf,
    byte_type* score_buf,
    const attribute_provider& doc,
    boost_t boost)
  : score_buf_(score_buf){
  scorers_.reserve(buckets.size());

  for (auto& entry: buckets) {
    assert(stats_buf);
    assert(entry.bucket); // ensured by order::prepared
    const auto& bucket = *entry.bucket;

    auto scorer = bucket.prepare_scorer(
      segment, field,
      stats_buf + entry.stats_offset,
      score_buf + entry.score_offset,
      doc, boost);

    if (scorer) {
      // skip empty scorers
      scorers_.emplace_back(std::move(scorer), &entry);
    }
  }
}

const byte_type* order::prepared::scorers::evaluate() const {
  for (auto& scorer : scorers_) {
    scorer.func();
  }
  return score_buf_;
}

void order::prepared::prepare_collectors(
    byte_type* stats_buf,
    const index_reader& index) const {
  for (auto& entry: order_) {
    assert(entry.bucket); // ensured by order::prepared
    entry.bucket->collect(stats_buf + entry.stats_offset, index, nullptr, nullptr);
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

}
