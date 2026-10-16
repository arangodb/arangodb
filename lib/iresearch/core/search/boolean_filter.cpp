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

#include "boolean_filter.hpp"

#include "conjunction.hpp"
#include "disjunction.hpp"
#include "exclusion.hpp"
#include "min_match_disjunction.hpp"
#include "prepared_state_visitor.hpp"
#include "search/boolean_query.hpp"

namespace irs {
namespace {

std::pair<const filter*, bool> optimize_not(const Not& node) {
  bool neg = true;
  const auto* inner = node.filter();
  while (inner != nullptr && inner->type() == type<Not>::id()) {
    neg = !neg;
    inner = DownCast<Not>(inner)->filter();
  }

  return std::pair{inner, neg};
}

}  // namespace
bool boolean_filter::equals(const filter& rhs) const noexcept {
  if (!filter::equals(rhs)) {
    return false;
  }
  const auto& typed_rhs = DownCast<boolean_filter>(rhs);
  return std::equal(
    begin(), end(), typed_rhs.begin(), typed_rhs.end(),
    [](const auto& lhs, const auto& rhs) { return *lhs == *rhs; });
}

filter::prepared::ptr boolean_filter::prepare(const PrepareContext& ctx) const {
  const auto size = filters_.size();

  if (IRS_UNLIKELY(size == 0)) {
    return prepared::empty();
  }

  if (size == 1) {
    auto* filter = filters_.front().get();
    IRS_ASSERT(filter);

    // FIXME(gnusi): let Not handle everything?
    if (filter->type() != irs::type<irs::Not>::id()) {
      return filter->prepare(ctx.Boost(boost()));
    }
  }

  // determine incl/excl parts
  std::vector<const filter*> incl;
  std::vector<const filter*> excl;

  AllDocsProvider::Ptr all_docs_zero_boost;
  AllDocsProvider::Ptr all_docs_no_boost;

  group_filters(all_docs_zero_boost, incl, excl);

  if (incl.empty() && !excl.empty()) {
    // single negative query case
    all_docs_no_boost = MakeAllDocsFilter(kNoBoost);
    incl.push_back(all_docs_no_boost.get());
  }

  return PrepareBoolean(incl, excl, ctx);
}

void boolean_filter::group_filters(AllDocsProvider::Ptr& all_docs_zero_boost,
                                   std::vector<const filter*>& incl,
                                   std::vector<const filter*>& excl) const {
  incl.reserve(size() / 2);
  excl.reserve(incl.capacity());

  const filter* empty_filter = nullptr;
  const auto is_or = type() == irs::type<Or>::id();
  for (const auto& filter : *this) {
    if (irs::type<Empty>::id() == filter->type()) {
      empty_filter = filter.get();
      continue;
    }
    if (irs::type<Not>::id() == filter->type()) {
      const auto res = optimize_not(DownCast<Not>(*filter));

      if (!res.first) {
        continue;
      }

      if (res.second) {
        if (!all_docs_zero_boost) {
          all_docs_zero_boost = MakeAllDocsFilter(0.F);
        }

        if (*all_docs_zero_boost == *res.first) {
          // not all -> empty result
          incl.clear();
          return;
        }
        excl.push_back(res.first);
        if (is_or) {
          // FIXME: this should have same boost as Not filter.
          // But for now we do not boost negation.
          incl.push_back(all_docs_zero_boost.get());
        }
      } else {
        incl.push_back(res.first);
      }
    } else {
      incl.push_back(filter.get());
    }
  }
  if (empty_filter != nullptr) {
    incl.push_back(empty_filter);
  }
}

filter::prepared::ptr And::PrepareBoolean(std::vector<const filter*>& incl,
                                          std::vector<const filter*>& excl,
                                          const PrepareContext& ctx) const {
  // optimization step
  //  if include group empty itself or has 'empty' -> this whole conjunction is
  //  empty
  if (incl.empty() || incl.back()->type() == irs::type<Empty>::id()) {
    return prepared::empty();
  }

  PrepareContext sub_ctx = ctx;

  // single node case
  if (1 == incl.size() && excl.empty()) {
    sub_ctx.boost *= boost();
    return incl.front()->prepare(sub_ctx);
  }

  auto cumulative_all = MakeAllDocsFilter(kNoBoost);
  score_t all_boost{0};
  size_t all_count{0};
  for (auto filter : incl) {
    if (*filter == *cumulative_all) {
      all_count++;
      all_boost += DownCast<FilterWithBoost>(*filter).boost();
    }
  }
  if (all_count != 0) {
    const auto non_all_count = incl.size() - all_count;
    auto it = std::remove_if(incl.begin(), incl.end(),
                             [&cumulative_all](const irs::filter* filter) {
                               return *cumulative_all == *filter;
                             });
    incl.erase(it, incl.end());
    // Here And differs from Or. Last 'All' should be left in include group only
    // if there is more than one filter of other type. Otherwise this another
    // filter could be container for boost from 'all' filters
    if (1 == non_all_count) {
      // let this last filter hold boost from all removed ones
      // so we aggregate in external boost values from removed all filters
      // If we will not optimize resulting boost will be:
      //   boost * OR_BOOST * ALL_BOOST + boost * OR_BOOST * LEFT_BOOST
      // We could adjust only 'boost' so we recalculate it as
      // new_boost =  ( boost * OR_BOOST * ALL_BOOST + boost * OR_BOOST *
      // LEFT_BOOST) / (OR_BOOST * LEFT_BOOST) so when filter will be executed
      // resulting boost will be: new_boost * OR_BOOST * LEFT_BOOST. If we
      // substitute new_boost back we will get ( boost * OR_BOOST * ALL_BOOST +
      // boost * OR_BOOST * LEFT_BOOST) - original non-optimized boost value
      auto left_boost = (*incl.begin())->BoostImpl();
      if (boost() != 0 && left_boost != 0 && !sub_ctx.scorers.empty()) {
        sub_ctx.boost = (sub_ctx.boost * boost() * all_boost +
                         sub_ctx.boost * boost() * left_boost) /
                        (left_boost * boost());
      } else {
        sub_ctx.boost = 0;
      }
    } else {
      // create new 'all' with boost from all removed
      cumulative_all->boost(all_boost);
      incl.push_back(cumulative_all.get());
    }
  }
  sub_ctx.boost *= this->boost();
  if (1 == incl.size() && excl.empty()) {
    // single node case
    return incl.front()->prepare(sub_ctx);
  }
  auto q = memory::make_tracked<AndQuery>(sub_ctx.memory);
  q->prepare(sub_ctx, merge_type(), incl, excl);
  return q;
}

filter::prepared::ptr Or::prepare(const PrepareContext& ctx) const {
  if (0 == min_match_count_) {  // only explicit 0 min match counts!
    // all conditions are satisfied
    return MakeAllDocsFilter(kNoBoost)->prepare(ctx.Boost(boost()));
  }

  return boolean_filter::prepare(ctx);
}

filter::prepared::ptr Or::PrepareBoolean(std::vector<const filter*>& incl,
                                         std::vector<const filter*>& excl,
                                         const PrepareContext& ctx) const {
  const PrepareContext sub_ctx = ctx.Boost(boost());

  if (0 == min_match_count_) {  // only explicit 0 min match counts!
    // all conditions are satisfied
    return MakeAllDocsFilter(kNoBoost)->prepare(sub_ctx);
  }

  if (!incl.empty() && incl.back()->type() == irs::type<Empty>::id()) {
    incl.pop_back();
  }

  if (incl.empty()) {
    return prepared::empty();
  }

  // single node case
  if (1 == incl.size() && excl.empty()) {
    return incl.front()->prepare(sub_ctx);
  }

  auto cumulative_all = MakeAllDocsFilter(kNoBoost);
  size_t optimized_match_count = 0;
  // Optimization steps

  score_t all_boost{0};
  size_t all_count{0};
  const irs::filter* incl_all{nullptr};
  for (auto filter : incl) {
    if (*filter == *cumulative_all) {
      all_count++;
      all_boost += DownCast<FilterWithBoost>(*filter).boost();
      incl_all = filter;
    }
  }
  if (all_count != 0) {
    if (sub_ctx.scorers.empty() && incl.size() > 1 &&
        min_match_count_ <= all_count) {
      // if we have at least one all in include group - all other filters are
      // not necessary in case there is no scoring and 'all' count satisfies
      // min_match
      IRS_ASSERT(incl_all != nullptr);
      incl.resize(1);
      incl.front() = incl_all;
      optimized_match_count = all_count - 1;
    } else {
      // Here Or differs from And. Last All should be left in include group
      auto it = std::remove_if(incl.begin(), incl.end(),
                               [&cumulative_all](const irs::filter* filter) {
                                 return *cumulative_all == *filter;
                               });
      incl.erase(it, incl.end());
      // create new 'all' with boost from all removed
      cumulative_all->boost(all_boost);
      incl.push_back(cumulative_all.get());
      optimized_match_count = all_count - 1;
    }
  }
  // check strictly less to not roll back to 0 min_match (we`ve handled this
  // case above!) single 'all' left -> it could contain boost we want to
  // preserve
  const auto adjusted_min_match = (optimized_match_count < min_match_count_)
                                    ? min_match_count_ - optimized_match_count
                                    : 1;

  if (adjusted_min_match > incl.size()) {
    // can't satisfy 'min_match_count' conditions
    // having only 'incl.size()' queries
    return prepared::empty();
  }

  if (1 == incl.size() && excl.empty()) {
    // single node case
    return incl.front()->prepare(sub_ctx);
  }

  IRS_ASSERT(adjusted_min_match > 0 && adjusted_min_match <= incl.size());

  memory::managed_ptr<BooleanQuery> q;
  if (adjusted_min_match == incl.size()) {
    q = memory::make_tracked<AndQuery>(sub_ctx.memory);
  } else if (1 == adjusted_min_match) {
    q = memory::make_tracked<OrQuery>(sub_ctx.memory);
  } else {  // min_match_count > 1 && min_match_count < incl.size()
    q = memory::make_tracked<MinMatchQuery>(sub_ctx.memory, adjusted_min_match);
  }

  q->prepare(sub_ctx, merge_type(), incl, excl);
  return q;
}

filter::prepared::ptr Not::prepare(const PrepareContext& ctx) const {
  const auto res = optimize_not(*this);

  if (!res.first) {
    return prepared::empty();
  }

  const PrepareContext sub_ctx = ctx.Boost(boost());

  if (res.second) {
    auto all_docs = MakeAllDocsFilter(kNoBoost);
    const std::array<const irs::filter*, 1> incl{all_docs.get()};
    const std::array<const irs::filter*, 1> excl{res.first};

    auto q = memory::make_tracked<AndQuery>(sub_ctx.memory);
    q->prepare(sub_ctx, ScoreMergeType::kSum, incl, excl);
    return q;
  }

  // negation has been optimized out
  return res.first->prepare(sub_ctx);
}

bool Not::equals(const irs::filter& rhs) const noexcept {
  if (!filter::equals(rhs)) {
    return false;
  }
  const auto& typed_rhs = DownCast<Not>(rhs);
  return (!empty() && !typed_rhs.empty() && *filter_ == *typed_rhs.filter_) ||
         (empty() && typed_rhs.empty());
}

}  // namespace irs
