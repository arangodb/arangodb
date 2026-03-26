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

#include "phrase_filter.hpp"

#include "index/field_meta.hpp"
#include "search/collectors.hpp"
#include "search/filter_visitor.hpp"
#include "search/phrase_iterator.hpp"
#include "search/phrase_query.hpp"
#include "search/prepared_state_visitor.hpp"
#include "search/states/phrase_state.hpp"
#include "search/states_cache.hpp"
#include "search/top_terms_collector.hpp"

namespace irs {
namespace {

struct TopTermsCollector final : filter_visitor {
  explicit TopTermsCollector(size_t size) : impl_{size} { IRS_ASSERT(size); }

  void prepare(const SubReader& segment, const term_reader& field,
               const seek_term_iterator& terms) final {
    impl_.prepare(segment, field, terms);
  }

  void visit(score_t boost) final { impl_.visit(boost); }

  field_visitor ToVisitor() {
    // TODO(MBkkt) we can avoid by_terms, but needs to change
    // top_terms_collector, to make it able keep equal elements
    irs::by_terms_options::search_terms terms;
    impl_.visit([&](top_term<score_t>& term) {
      terms.emplace(std::move(term.term), term.key);
    });
    return [terms = std::move(terms)](const SubReader& segment,
                                      const term_reader& field,
                                      filter_visitor& visitor) {
      return by_terms::visit(segment, field, terms, visitor);
    };
  }

 private:
  top_terms_collector<top_term<score_t>> impl_;
};

struct GetVisitor {
  field_visitor operator()(const by_term_options& part) const {
    return [term = bytes_view(part.term)](const SubReader& segment,
                                          const term_reader& field,
                                          filter_visitor& visitor) {
      return by_term::visit(segment, field, term, visitor);
    };
  }

  field_visitor operator()(const by_prefix_options& part) const {
    return [term = bytes_view(part.term)](const SubReader& segment,
                                          const term_reader& field,
                                          filter_visitor& visitor) {
      return by_prefix::visit(segment, field, term, visitor);
    };
  }

  field_visitor operator()(const by_wildcard_options& part) const {
    return by_wildcard::visitor(part.term);
  }

  field_visitor operator()(const by_edit_distance_options& part) const {
    if (part.max_terms != 0) {
      return {};
    }
    return by_edit_distance::visitor(part);
  }

  field_visitor operator()(const by_terms_options& part) const {
    return
      [terms = &part.terms](const SubReader& segment, const term_reader& field,
                            filter_visitor& visitor) {
        return by_terms::visit(segment, field, *terms, visitor);
      };
  }

  field_visitor operator()(const by_range_options& part) const {
    return
      [range = &part.range](const SubReader& segment, const term_reader& field,
                            filter_visitor& visitor) {
        return by_range::visit(segment, field, *range, visitor);
      };
  }
};

struct PrepareVisitor : util::noncopyable {
  auto operator()(const by_term_options& opts) const {
    return by_term::prepare(ctx, field, opts.term);
  }

  auto operator()(const by_prefix_options& part) const {
    return by_prefix::prepare(ctx, field, part.term, part.scored_terms_limit);
  }

  auto operator()(const by_wildcard_options& part) const {
    return by_wildcard::prepare(ctx, field, part.term, part.scored_terms_limit);
  }

  auto operator()(const by_edit_distance_options& part) const {
    return by_edit_distance::prepare(ctx, field, part.term, part.max_terms,
                                     part.max_distance, part.provider,
                                     part.with_transpositions, part.prefix);
  }

  filter::prepared::ptr operator()(const by_terms_options&) const { return {}; }

  auto operator()(const by_range_options& part) const {
    return by_range::prepare(ctx, field, part.range, part.scored_terms_limit);
  }

  PrepareVisitor(const PrepareContext& ctx, std::string_view field) noexcept
    : ctx{ctx}, field{field} {}

  const PrepareContext& ctx;
  const std::string_view field;
};

// Filter visitor for phrase queries
template<typename PhraseStates>
class phrase_term_visitor final : public filter_visitor,
                                  private util::noncopyable {
 public:
  explicit phrase_term_visitor(PhraseStates& phrase_states) noexcept
    : phrase_states_(phrase_states) {}

  void prepare(const SubReader& segment, const term_reader& field,
               const seek_term_iterator& terms) noexcept final {
    segment_ = &segment;
    reader_ = &field;
    terms_ = &terms;
    found_ = true;
  }

  void visit(score_t boost) final {
    IRS_ASSERT(terms_ && collectors_ && segment_ && reader_);

    // disallow negative boost
    boost = std::max(0.f, boost);

    if (stats_size_ <= term_offset_) {
      // variadic phrase case
      collectors_->push_back();
      IRS_ASSERT(stats_size_ == term_offset_);
      ++stats_size_;
      volatile_boost_ |= (boost != kNoBoost);
    }

    collectors_->collect(*segment_, *reader_, term_offset_++, *terms_);
    phrase_states_.emplace_back(terms_->cookie(), boost);
  }

  void reset() noexcept { volatile_boost_ = false; }

  void reset(term_collectors& collectors) noexcept {
    found_ = false;
    terms_ = nullptr;
    term_offset_ = 0;
    collectors_ = &collectors;
    stats_size_ = collectors.size();
  }

  bool found() const noexcept { return found_; }

  bool volatile_boost() const noexcept { return volatile_boost_; }

 private:
  size_t term_offset_ = 0;
  size_t stats_size_ = 0;
  const SubReader* segment_{};
  const term_reader* reader_{};
  PhraseStates& phrase_states_;
  term_collectors* collectors_ = nullptr;
  const seek_term_iterator* terms_ = nullptr;
  bool found_ = false;
  bool volatile_boost_ = false;
};

bool Valid(const term_reader* reader) noexcept {
  static_assert(FixedPhraseQuery::kRequiredFeatures ==
                VariadicPhraseQuery::kRequiredFeatures);
  // check field reader exists with required features
  return reader != nullptr && (reader->meta().index_features &
                               FixedPhraseQuery::kRequiredFeatures) ==
                                FixedPhraseQuery::kRequiredFeatures;
}

filter::prepared::ptr FixedPrepareCollect(const PrepareContext& ctx,
                                          std::string_view field,
                                          const by_phrase_options& options) {
  const auto phrase_size = options.size();
  const auto is_ord_empty = ctx.scorers.empty();

  // stats collectors
  field_collectors field_stats(ctx.scorers);
  term_collectors term_stats(ctx.scorers, phrase_size);

  // per segment phrase states
  FixedPhraseQuery::states_t phrase_states{ctx.memory, ctx.index.size()};

  // per segment phrase terms
  FixedPhraseState::Terms phrase_terms{{ctx.memory}};
  phrase_terms.reserve(phrase_size);

  // iterate over the segments
  phrase_term_visitor<decltype(phrase_terms)> ptv(phrase_terms);

  for (const auto& segment : ctx.index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);
    if (!Valid(reader)) {
      continue;
    }

    // collect field statistics once per segment
    field_stats.collect(segment, *reader);
    ptv.reset(term_stats);

    for (const auto& word : options) {
      IRS_ASSERT(std::get_if<by_term_options>(&word.second));
      by_term::visit(segment, *reader,
                     std::get<by_term_options>(word.second).term, ptv);
      if (!ptv.found() && is_ord_empty) {
        break;
      }
    }

    // we have not found all needed terms
    if (phrase_terms.size() != phrase_size) {
      phrase_terms.clear();
      continue;
    }

    auto& state = phrase_states.insert(segment);
    state.terms = std::move(phrase_terms);
    state.reader = reader;

    phrase_terms.reserve(phrase_size);
  }

#ifndef IRESEARCH_TEST  // TODO(MBkkt) adjust tests
  if (phrase_states.empty()) {
    return filter::prepared::empty();
  }
#endif

  // offset of the first term in a phrase
  IRS_ASSERT(!options.empty());
  const size_t base_offset = options.begin()->first;

  // finish stats
  bstring stats(ctx.scorers.stats_size(), 0);  // aggregated phrase stats
  auto* stats_buf = stats.data();

  FixedPhraseQuery::positions_t positions(phrase_size);
  auto pos_itr = positions.begin();

  size_t term_idx = 0;
  for (const auto& term : options) {
    *pos_itr = static_cast<position::value_t>(term.first - base_offset);
    term_stats.finish(stats_buf, term_idx, field_stats, ctx.index);
    ++pos_itr;
    ++term_idx;
  }

  return memory::make_tracked<FixedPhraseQuery>(
    ctx.memory, std::move(phrase_states), std::move(positions),
    std::move(stats), ctx.boost);
}

filter::prepared::ptr VariadicPrepareCollect(const PrepareContext& ctx,
                                             std::string_view field,
                                             const by_phrase_options& options) {
  const auto phrase_size = options.size();

  // stats collectors
  field_collectors field_stats{ctx.scorers};

  std::vector<field_visitor> phrase_part_visitors;
  phrase_part_visitors.reserve(phrase_size);
  std::vector<term_collectors> phrase_part_stats;
  phrase_part_stats.reserve(phrase_size);

  std::vector<field_visitor*> all_terms_visitors;
  std::vector<TopTermsCollector> top_terms_collectors;

  for (const auto& word : options) {
    phrase_part_stats.emplace_back(ctx.scorers, 0);
    auto& visitor =
      phrase_part_visitors.emplace_back(std::visit(GetVisitor{}, word.second));
    if (!visitor) {
      auto& opts = std::get<by_edit_distance_options>(word.second);
      visitor = irs::by_edit_distance::visitor(opts);
      all_terms_visitors.push_back(&visitor);
      top_terms_collectors.emplace_back(opts.max_terms);
    }
  }

  if (!all_terms_visitors.empty()) {
    // TODO(MBkkt) we should move all terms search to here
    // And make second loop for index only to make correct order of terms
    for (const auto& segment : ctx.index) {
      // get term dictionary for field
      const auto* reader = segment.field(field);
      if (!Valid(reader)) {
        continue;
      }
      auto it = top_terms_collectors.begin();
      for (auto* visitor : all_terms_visitors) {
        (*visitor)(segment, *reader, *it++);
      }
    }
    auto it = top_terms_collectors.begin();
    for (auto* visitor : all_terms_visitors) {
      *visitor = it++->ToVisitor();
    }
  }

  // per segment phrase states
  VariadicPhraseQuery::states_t phrase_states{ctx.memory, ctx.index.size()};

  // per segment phrase terms: number of terms per part
  ManagedVector<size_t> num_terms(phrase_size, {ctx.memory});
  VariadicPhraseState::Terms phrase_terms{{ctx.memory}};
  // reserve space for at least 1 term per part
  phrase_terms.reserve(phrase_size);

  // iterate over the segments
  const auto is_ord_empty = ctx.scorers.empty();

  phrase_term_visitor<decltype(phrase_terms)> ptv(phrase_terms);

  for (const auto& segment : ctx.index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);
    if (!Valid(reader)) {
      continue;
    }

    // collect field statistics once per segment
    field_stats.collect(segment, *reader);
    ptv.reset();  // reset boost volaitility mark

    size_t found_parts = 0;
    for (const auto& visitor : phrase_part_visitors) {
      const auto was_terms_count = phrase_terms.size();
      ptv.reset(phrase_part_stats[found_parts]);
      visitor(segment, *reader, ptv);
      const auto new_terms_count = phrase_terms.size() - was_terms_count;
      // TODO(MBkkt) Avoid unnecessary work for min_match > 1 queries
      if (new_terms_count != 0) {
        num_terms[found_parts++] = new_terms_count;
      } else if (is_ord_empty) {
        break;
      }
    }

    // we have not found all needed terms
    if (found_parts != phrase_size) {
      phrase_terms.clear();
      continue;
    }

    auto& state = phrase_states.insert(segment);
    state.terms = std::move(phrase_terms);
    state.num_terms = std::move(num_terms);
    state.reader = reader;
    state.volatile_boost = !is_ord_empty && ptv.volatile_boost();
    IRS_ASSERT(phrase_size == state.num_terms.size());

    phrase_terms.reserve(phrase_size);
    // reserve space for at least 1 term per part
    num_terms.resize(phrase_size);
  }

#ifndef IRESEARCH_TEST  // TODO(MBkkt) adjust tests
  if (phrase_states.empty()) {
    return filter::prepared::empty();
  }
#endif

  // offset of the first term in a phrase
  IRS_ASSERT(!options.empty());
  const auto base_offset = options.begin()->first;

  // finish stats
  IRS_ASSERT(phrase_size == phrase_part_stats.size());
  bstring stats(ctx.scorers.stats_size(), 0);  // aggregated phrase stats
  auto* stats_buf = stats.data();
  auto collector = phrase_part_stats.begin();

  VariadicPhraseQuery::positions_t positions(phrase_size);
  auto position = positions.begin();

  for (const auto& term : options) {
    *position++ = static_cast<position::value_t>(term.first - base_offset);
    for (size_t i = 0, size = collector->size(); i < size; ++i) {
      collector->finish(stats_buf, i, field_stats, ctx.index);
    }
    ++collector;
  }

  return memory::make_tracked<VariadicPhraseQuery>(
    ctx.memory, std::move(phrase_states), std::move(positions),
    std::move(stats), ctx.boost);
}

}  // namespace

filter::prepared::ptr by_phrase::Prepare(const PrepareContext& ctx,
                                         std::string_view field,
                                         const by_phrase_options& options) {
  if (field.empty() || options.empty()) {
    // empty field or phrase
    return prepared::empty();
  }

  if (1 == options.size()) {
    auto query =
      std::visit(PrepareVisitor{ctx, field}, options.begin()->second);
    if (query) {
      return query;
    }
  }

  // prepare phrase stats (collector for each term)
  if (options.simple()) {
    return FixedPrepareCollect(ctx, field, options);
  }

  return VariadicPrepareCollect(ctx, field, options);
}

}  // namespace irs
