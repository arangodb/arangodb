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

#pragma once

#include <vector>

#include "search/exclusion.hpp"
#include "search/filter.hpp"

namespace irs {

// Base class for boolean queries
class BooleanQuery : public filter::prepared {
 public:
  using queries_t = ManagedVector<filter::prepared::ptr>;
  using iterator = queries_t::const_iterator;

  doc_iterator::ptr execute(const ExecutionContext& ctx) const final;

  void visit(const irs::SubReader& segment, irs::PreparedStateVisitor& visitor,
             score_t boost) const final;

  score_t boost() const noexcept final { return boost_; }

  void prepare(const PrepareContext& ctx, ScoreMergeType merge_type,
               queries_t queries, size_t exclude_start);

  void prepare(const PrepareContext& ctx, ScoreMergeType merge_type,
               std::span<const filter* const> incl,
               std::span<const filter* const> excl);

  iterator begin() const { return queries_.begin(); }
  iterator excl_begin() const { return begin() + excl_; }
  iterator end() const { return queries_.end(); }

  bool empty() const { return queries_.empty(); }
  size_t size() const { return queries_.size(); }

 protected:
  virtual doc_iterator::ptr execute(const ExecutionContext& ctx, iterator begin,
                                    iterator end) const = 0;

  ScoreMergeType merge_type() const noexcept { return merge_type_; }

 private:
  // 0..excl_-1 - included queries
  // excl_..queries.end() - excluded queries
  queries_t queries_;
  // index of the first excluded query
  size_t excl_ = 0;
  ScoreMergeType merge_type_ = ScoreMergeType::kSum;
  score_t boost_ = kNoBoost;
};

// Represent a set of queries joint by "And"
class AndQuery : public BooleanQuery {
 public:
  doc_iterator::ptr execute(const ExecutionContext& ctx, iterator begin,
                            iterator end) const final;
};

// Represent a set of queries joint by "Or"
class OrQuery : public BooleanQuery {
 public:
  doc_iterator::ptr execute(const ExecutionContext& ctx, iterator begin,
                            iterator end) const final;
};

// Represent a set of queries joint by "Or" with the specified
// minimum number of clauses that should satisfy criteria
class MinMatchQuery : public BooleanQuery {
 public:
  explicit MinMatchQuery(size_t min_match_count) noexcept
    : min_match_count_{min_match_count} {
    IRS_ASSERT(min_match_count_ > 1);
  }

  doc_iterator::ptr execute(const ExecutionContext& ctx, iterator begin,
                            iterator end) const final;

 private:
  size_t min_match_count_;
};

}  // namespace irs
