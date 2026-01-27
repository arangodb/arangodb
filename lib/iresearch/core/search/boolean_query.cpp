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

#include "search/boolean_query.hpp"

#include "search/conjunction.hpp"
#include "search/disjunction.hpp"
#include "search/prepared_state_visitor.hpp"

namespace irs {
namespace {

template<bool Conjunction, typename It>
irs::ScoreAdapters MakeScoreAdapters(const irs::ExecutionContext& ctx, It begin,
                                     It end) {
  IRS_ASSERT(begin <= end);
  const size_t size = std::distance(begin, end);
  irs::ScoreAdapters itrs;
  itrs.reserve(size);
  if (Conjunction || size > 1) {
    ctx.wand.root = false;
    // TODO(MBkkt) ctx.wand.strict = true;
    // We couldn't do this for few reasons:
    // 1. It's small chance that we will use just term iterator (or + eof)
    // 2. I'm not sure about precision
  }
  do {
    auto docs = (*begin)->execute(ctx);
    ++begin;

    // filter out empty iterators
    if (irs::doc_limits::eof(docs->value())) {
      if constexpr (Conjunction) {
        return {};
      } else {
        continue;
      }
    }

    itrs.emplace_back(std::move(docs));
  } while (begin != end);

  return itrs;
}

// Returns disjunction iterator created from the specified queries
template<typename QueryIterator, typename... Args>
irs::doc_iterator::ptr make_disjunction(const irs::ExecutionContext& ctx,
                                        irs::ScoreMergeType merge_type,
                                        QueryIterator begin, QueryIterator end,
                                        Args&&... args) {
  IRS_ASSERT(begin <= end);
  const size_t size = std::distance(begin, end);
  // check the size before the execution
  if (0 == size) {
    // empty or unreachable search criteria
    return irs::doc_iterator::empty();
  }

  auto itrs = MakeScoreAdapters<false>(ctx, begin, end);
  if (itrs.empty()) {
    return irs::doc_iterator::empty();
  }

  return irs::ResoveMergeType(
    merge_type, ctx.scorers.buckets().size(),
    [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
      using disjunction_t =
        irs::disjunction_iterator<irs::doc_iterator::ptr, A>;

      return irs::MakeDisjunction<disjunction_t>(ctx.wand, std::move(itrs),
                                                 std::forward<A>(aggregator),
                                                 std::forward<Args>(args)...);
    });
}

// Returns conjunction iterator created from the specified queries
template<typename QueryIterator, typename... Args>
irs::doc_iterator::ptr make_conjunction(const irs::ExecutionContext& ctx,
                                        irs::ScoreMergeType merge_type,
                                        QueryIterator begin, QueryIterator end,
                                        Args&&... args) {
  IRS_ASSERT(begin <= end);
  const size_t size = std::distance(begin, end);
  // check size before the execution
  switch (size) {
    case 0:
      return irs::doc_iterator::empty();
    case 1:
      return (*begin)->execute(ctx);
  }

  auto itrs = MakeScoreAdapters<true>(ctx, begin, end);
  if (itrs.empty()) {
    return irs::doc_iterator::empty();
  }

  return irs::ResoveMergeType(
    merge_type, ctx.scorers.buckets().size(),
    [&]<typename A>(A&& aggregator) -> irs::doc_iterator::ptr {
      return irs::MakeConjunction(ctx.wand, std::forward<A>(aggregator),
                                  std::move(itrs), std::forward<Args>(args)...);
    });
}

}  // namespace

doc_iterator::ptr BooleanQuery::execute(const ExecutionContext& ctx) const {
  if (empty()) {
    return doc_iterator::empty();
  }

  IRS_ASSERT(excl_);
  const auto excl_begin = this->excl_begin();
  const auto end = this->end();

  auto incl = execute(ctx, begin(), excl_begin);

  if (excl_begin == end) {
    return incl;
  }

  // exclusion part does not affect scoring at all
  auto excl = make_disjunction(
    {.segment = ctx.segment, .scorers = Scorers::kUnordered, .ctx = ctx.ctx},
    irs::ScoreMergeType::kNoop, excl_begin, end);

  // got empty iterator for excluded
  if (doc_limits::eof(excl->value())) {
    // pure conjunction/disjunction
    return incl;
  }

  return memory::make_managed<exclusion>(std::move(incl), std::move(excl));
}

void BooleanQuery::visit(const irs::SubReader& segment,
                         irs::PreparedStateVisitor& visitor,
                         score_t boost) const {
  boost *= boost_;

  if (!visitor.Visit(*this, boost)) {
    return;
  }

  // FIXME(gnusi): visit exclude group?
  for (auto it = begin(), end = excl_begin(); it != end; ++it) {
    (*it)->visit(segment, visitor, boost);
  }
}

void BooleanQuery::prepare(const PrepareContext& ctx, ScoreMergeType merge_type,
                           queries_t queries, size_t exclude_start) {
  // apply boost to the current node
  boost_ *= ctx.boost;
  // nothrow block
  queries_ = std::move(queries);
  excl_ = exclude_start;
  merge_type_ = merge_type;
}

void BooleanQuery::prepare(const PrepareContext& ctx, ScoreMergeType merge_type,
                           std::span<const filter* const> incl,
                           std::span<const filter* const> excl) {
  queries_t queries{{ctx.memory}};
  queries.reserve(incl.size() + excl.size());
  // prepare included
  for (const auto* filter : incl) {
    queries.emplace_back(filter->prepare(ctx));
  }
  // prepare excluded
  for (const auto* filter : excl) {
    // exclusion part does not affect scoring at all
    queries.emplace_back(filter->prepare({
      .index = ctx.index,
      .memory = ctx.memory,
      .ctx = ctx.ctx,
    }));
  }
  prepare(ctx, merge_type, std::move(queries), incl.size());
}

doc_iterator::ptr AndQuery::execute(const ExecutionContext& ctx, iterator begin,
                                    iterator end) const {
  return make_conjunction(ctx, merge_type(), begin, end);
}

doc_iterator::ptr OrQuery::execute(const ExecutionContext& ctx, iterator begin,
                                   iterator end) const {
  return make_disjunction(ctx, merge_type(), begin, end);
}

doc_iterator::ptr MinMatchQuery::execute(const ExecutionContext& ctx,
                                         iterator begin, iterator end) const {
  IRS_ASSERT(std::distance(begin, end) >= 0);
  const auto size = size_t(std::distance(begin, end));

  // 1 <= min_match_count
  size_t min_match_count = std::max(size_t{1}, min_match_count_);

  // check the size before the execution
  if (0 == size || min_match_count > size) {
    // empty or unreachable search criteria
    return doc_iterator::empty();
  } else if (min_match_count == size) {
    // pure conjunction
    return make_conjunction(ctx, merge_type(), begin, end);
  }

  // min_match_count <= size
  min_match_count = std::min(size, min_match_count);

  auto itrs = MakeScoreAdapters<false>(ctx, begin, end);
  if (itrs.empty()) {
    return irs::doc_iterator::empty();
  }

  return ResoveMergeType(merge_type(), ctx.scorers.buckets().size(),
                         [&]<typename A>(A&& aggregator) -> doc_iterator::ptr {
                           // FIXME(gnusi): use FAST version
                           using disjunction_t =
                             min_match_iterator<doc_iterator::ptr, A>;

                           return MakeWeakDisjunction<disjunction_t, A>(
                             ctx.wand, std::move(itrs), min_match_count,
                             std::forward<A>(aggregator));
                         });
}

}  // namespace irs
