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

#include "collectors.hpp"

namespace {

using namespace irs;

struct noop_field_collector final : FieldCollector {
  void collect(const SubReader&, const term_reader&) final {}
  void reset() final {}
  void collect(bytes_view) final {}
  void write(data_output&) const final {}
};

struct noop_term_collector final : TermCollector {
  void collect(const SubReader&, const term_reader&,
               const attribute_provider&) final {}
  void reset() final {}
  void collect(bytes_view) final {}
  void write(data_output&) const final {}
};

static noop_field_collector NOOP_FIELD_STATS;
static noop_term_collector NOOP_TERM_STATS;

}  // namespace

namespace irs {

field_collector_wrapper::collector_type&
field_collector_wrapper::noop() noexcept {
  return NOOP_FIELD_STATS;
}

field_collectors::field_collectors(const Scorers& order)
  : collectors_base<field_collector_wrapper>(order.buckets().size(), order) {
  auto collectors = collectors_.begin();
  for (auto& bucket : order.buckets()) {
    *collectors = bucket.bucket->prepare_field_collector();
    IRS_ASSERT(*collectors);  // ensured by wrapper
    ++collectors;
  }
  IRS_ASSERT(collectors == collectors_.end());
}

void field_collectors::collect(const SubReader& segment,
                               const term_reader& field) const {
  switch (collectors_.size()) {
    case 0:
      return;
    case 1:
      collectors_.front()->collect(segment, field);
      return;
    case 2:
      collectors_.front()->collect(segment, field);
      collectors_.back()->collect(segment, field);
      return;
    default:
      for (auto& collector : collectors_) {
        collector->collect(segment, field);
      }
  }
}

void field_collectors::finish(byte_type* stats_buf) const {
  // special case where term statistics collection is not applicable
  // e.g. by_column_existence filter
  IRS_ASSERT(buckets_.size() == collectors_.size());

  for (size_t i = 0, count = collectors_.size(); i < count; ++i) {
    auto& sort = buckets_[i];
    IRS_ASSERT(sort.bucket);  // ensured by order::prepare

    sort.bucket->collect(
      stats_buf + sort.stats_offset,  // where stats for bucket start
      collectors_[i].get(), nullptr);
  }
}

term_collector_wrapper::collector_type&
term_collector_wrapper::noop() noexcept {
  return NOOP_TERM_STATS;
}

term_collectors::term_collectors(const Scorers& buckets, size_t size)
  : collectors_base<term_collector_wrapper>(buckets.buckets().size() * size,
                                            buckets) {
  // add term collectors from each bucket
  // layout order [t0.b0, t0.b1, ... t0.bN, t1.b0, t1.b1 ... tM.BN]
  auto begin = collectors_.begin();
  auto end = collectors_.end();
  for (; begin != end;) {
    for (auto& entry : buckets.buckets()) {
      IRS_ASSERT(entry.bucket);  // ensured by order::prepare

      *begin = entry.bucket->prepare_term_collector();
      IRS_ASSERT(*begin);  // ensured by wrapper
      ++begin;
    }
  }
  IRS_ASSERT(begin == collectors_.end());
}

void term_collectors::collect(const SubReader& segment,
                              const term_reader& field, size_t term_idx,
                              const attribute_provider& attrs) const {
  const size_t count = buckets_.size();

  switch (count) {
    case 0:
      return;
    case 1: {
      IRS_ASSERT(term_idx < collectors_.size());
      IRS_ASSERT(collectors_[term_idx]);  // enforced by wrapper
      collectors_[term_idx]->collect(segment, field, attrs);
      return;
    }
    case 2: {
      IRS_ASSERT(term_idx + 1 < collectors_.size());
      IRS_ASSERT(collectors_[term_idx]);      // enforced by wrapper
      IRS_ASSERT(collectors_[term_idx + 1]);  // enforced by wrapper
      collectors_[term_idx]->collect(segment, field, attrs);
      collectors_[term_idx + 1]->collect(segment, field, attrs);
      return;
    }
    default: {
      const size_t term_offset_count = term_idx * count;
      for (size_t i = 0; i < count; ++i) {
        const auto idx = term_offset_count + i;
        IRS_ASSERT(
          idx <
          collectors_.size());  // enforced by allocation in the constructor
        IRS_ASSERT(collectors_[idx]);  // enforced by wrapper

        collectors_[idx]->collect(segment, field, attrs);
      }
      return;
    }
  }
}

size_t term_collectors::push_back() {
  const size_t size = buckets_.size();
  IRS_ASSERT(0 == size || 0 == collectors_.size() % size);

  switch (size) {
    case 0:
      return 0;
    case 1: {
      const auto term_offset = collectors_.size();
      IRS_ASSERT(buckets_.front().bucket);  // ensured by order::prepare
      collectors_.emplace_back(
        buckets_.front().bucket->prepare_term_collector());
      return term_offset;
    }
    case 2: {
      const auto term_offset = collectors_.size() / 2;
      IRS_ASSERT(buckets_.front().bucket);  // ensured by order::prepare
      collectors_.emplace_back(
        buckets_.front().bucket->prepare_term_collector());
      IRS_ASSERT(buckets_.back().bucket);  // ensured by order::prepare
      collectors_.emplace_back(
        buckets_.back().bucket->prepare_term_collector());
      return term_offset;
    }
    default: {
      const auto term_offset = collectors_.size() / size;
      collectors_.reserve(collectors_.size() + size);
      for (auto& entry : buckets_) {
        IRS_ASSERT(entry.bucket);  // ensured by order::prepare
        collectors_.emplace_back(entry.bucket->prepare_term_collector());
      }
      return term_offset;
    }
  }
}

void term_collectors::finish(byte_type* stats_buf, size_t term_idx,
                             const field_collectors& field_collectors,
                             const IndexReader& /*index*/) const {
  const auto bucket_count = buckets_.size();

  switch (bucket_count) {
    case 0:
      break;
    case 1: {
      IRS_ASSERT(field_collectors.front());
      IRS_ASSERT(buckets_.front().bucket);
      buckets_.front().bucket->collect(
        stats_buf + buckets_.front().stats_offset, field_collectors.front(),
        collectors_[term_idx].get());
    } break;
    case 2: {
      term_idx *= bucket_count;

      IRS_ASSERT(field_collectors.front());
      IRS_ASSERT(buckets_.front().bucket);
      buckets_.front().bucket->collect(
        stats_buf + buckets_.front().stats_offset, field_collectors.front(),
        collectors_[term_idx].get());

      IRS_ASSERT(field_collectors.back());
      IRS_ASSERT(buckets_.back().bucket);
      buckets_.back().bucket->collect(stats_buf + buckets_.back().stats_offset,
                                      field_collectors.back(),
                                      collectors_[term_idx + 1].get());
    } break;
    default: {
      term_idx *= bucket_count;

      auto begin = field_collectors.begin();
      for (auto& bucket : buckets_) {
        bucket.bucket->collect(stats_buf + bucket.stats_offset, begin->get(),
                               collectors_[term_idx++].get());
        ++begin;
      }
    } break;
  }
}

}  // namespace irs
