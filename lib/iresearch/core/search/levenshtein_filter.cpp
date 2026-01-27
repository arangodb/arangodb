////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "levenshtein_filter.hpp"

#include "index/index_reader.hpp"
#include "search/all_terms_collector.hpp"
#include "search/filter_visitor.hpp"
#include "search/limited_sample_collector.hpp"
#include "search/multiterm_query.hpp"
#include "search/term_filter.hpp"
#include "search/top_terms_collector.hpp"
#include "shared.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/levenshtein_default_pdp.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/std.hpp"
#include "utils/utf8_utils.hpp"

namespace irs {
namespace {

////////////////////////////////////////////////////////////////////////////////
/// @returns levenshtein similarity
////////////////////////////////////////////////////////////////////////////////
IRS_FORCE_INLINE score_t similarity(uint32_t distance, uint32_t size) noexcept {
  IRS_ASSERT(size);

  static_assert(sizeof(score_t) == sizeof(uint32_t));

  return 1.f - static_cast<score_t>(distance) / static_cast<score_t>(size);
}

template<typename Invalid, typename Term, typename Levenshtein>
inline auto executeLevenshtein(uint8_t max_distance,
                               by_edit_distance_options::pdp_f provider,
                               bool with_transpositions,
                               const bytes_view prefix, const bytes_view target,
                               Invalid&& inv, Term&& t, Levenshtein&& lev) {
  if (!provider) {
    provider = &default_pdp;
  }

  if (0 == max_distance) {
    return t();
  }

  IRS_ASSERT(provider);
  const auto& d = (*provider)(max_distance, with_transpositions);

  if (!d) {
    return inv();
  }

  return lev(d, prefix, target);
}

template<typename StatesType>
struct aggregated_stats_visitor : util::noncopyable {
  aggregated_stats_visitor(StatesType& states,
                           const term_collectors& term_stats) noexcept
    : term_stats(term_stats), states(states) {}

  void operator()(const irs::SubReader& segment, const irs::term_reader& field,
                  uint32_t docs_count) const {
    this->segment = &segment;
    this->field = &field;
    state = &states.insert(segment);
    state->reader = &field;
    state->scored_states_estimation += docs_count;
  }

  void operator()(seek_cookie::ptr& cookie) const {
    IRS_ASSERT(segment);
    IRS_ASSERT(field);
    term_stats.collect(*segment, *field, 0, *cookie);
    state->scored_states.emplace_back(std::move(cookie), 0, boost);
  }

  const term_collectors& term_stats;
  StatesType& states;
  mutable typename StatesType::state_type* state{};
  mutable const SubReader* segment{};
  mutable const term_reader* field{};
  score_t boost{irs::kNoBoost};
};

class top_terms_collector
  : public irs::top_terms_collector<top_term_state<score_t>> {
 public:
  using base_type = irs::top_terms_collector<top_term_state<score_t>>;

  top_terms_collector(size_t size, field_collectors& field_stats)
    : base_type(size), field_stats_(field_stats) {}

  void prepare(const SubReader& segment, const term_reader& field,
               const seek_term_iterator& terms) {
    field_stats_.collect(segment, field);
    base_type::prepare(segment, field, terms);
  }

 private:
  field_collectors& field_stats_;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief visitation logic for levenshtein filter
/// @param segment segment reader
/// @param field term reader
/// @param matcher input matcher
/// @param visitor visitor
//////////////////////////////////////////////////////////////////////////////
template<typename Visitor>
void VisitImpl(const SubReader& segment, const term_reader& reader,
               const byte_type no_distance, const uint32_t utf8_target_size,
               automaton_table_matcher& matcher, Visitor&& visitor) {
  IRS_ASSERT(fst::kError != matcher.Properties(0));
  auto terms = reader.iterator(matcher);

  if (IRS_UNLIKELY(!terms)) {
    return;
  }

  if (terms->next()) {
    auto* payload = irs::get<irs::payload>(*terms);

    const byte_type* distance{&no_distance};
    if (payload && !payload->value.empty()) {
      distance = &payload->value.front();
    }

    visitor.prepare(segment, reader, *terms);

    do {
      terms->read();

      const auto utf8_value_size =
        static_cast<uint32_t>(utf8_utils::Length(terms->value()));
      const auto boost =
        similarity(*distance, std::min(utf8_value_size, utf8_target_size));

      visitor.visit(boost);
    } while (terms->next());
  }
}

template<typename Collector>
bool collect_terms(const IndexReader& index, std::string_view field,
                   bytes_view prefix, bytes_view term,
                   const parametric_description& d, Collector& collector) {
  const auto acceptor = make_levenshtein_automaton(d, prefix, term);

  if (!Validate(acceptor)) {
    return false;
  }

  auto matcher = MakeAutomatonMatcher(acceptor);
  const auto utf8_term_size =
    std::max(1U, static_cast<uint32_t>(utf8_utils::Length(prefix) +
                                       utf8_utils::Length(term)));
  const uint8_t max_distance = d.max_distance() + 1;

  for (auto& segment : index) {
    if (auto* reader = segment.field(field); reader) {
      VisitImpl(segment, *reader, max_distance, utf8_term_size, matcher,
                collector);
    }
  }

  return true;
}

filter::prepared::ptr prepare_levenshtein_filter(
  const PrepareContext& ctx, std::string_view field, bytes_view prefix,
  bytes_view term, size_t terms_limit, const parametric_description& d) {
  field_collectors field_stats{ctx.scorers};
  term_collectors term_stats{ctx.scorers, 1};
  MultiTermQuery::States states{ctx.memory, ctx.index.size()};

  if (!terms_limit) {
    all_terms_collector term_collector{states, field_stats, term_stats};
    term_collector.stat_index(0);  // aggregate stats from different terms

    if (!collect_terms(ctx.index, field, prefix, term, d, term_collector)) {
      return filter::prepared::empty();
    }
  } else {
    top_terms_collector term_collector(terms_limit, field_stats);

    if (!collect_terms(ctx.index, field, prefix, term, d, term_collector)) {
      return filter::prepared::empty();
    }

    aggregated_stats_visitor aggregate_stats{states, term_stats};
    term_collector.visit([&aggregate_stats](top_term_state<score_t>& state) {
      aggregate_stats.boost = std::max(0.f, state.key);
      state.visit(aggregate_stats);
    });
  }

  MultiTermQuery::Stats stats(
    1, MultiTermQuery::Stats::allocator_type{ctx.memory});
  stats.back().resize(ctx.scorers.stats_size(), 0);
  auto* stats_buf = stats[0].data();
  term_stats.finish(stats_buf, 0, field_stats, ctx.index);

  return memory::make_tracked<MultiTermQuery>(ctx.memory, std::move(states),
                                              std::move(stats), ctx.boost,
                                              ScoreMergeType::kMax, size_t{1});
}

}  // namespace

field_visitor by_edit_distance::visitor(
  const by_edit_distance_all_options& opts) {
  return executeLevenshtein(
    opts.max_distance, opts.provider, opts.with_transpositions, opts.prefix,
    opts.term,
    []() -> field_visitor {
      return [](const SubReader&, const term_reader&, filter_visitor&) {};
    },
    [&opts]() -> field_visitor {
      // must copy term as it may point to temporary string
      return [target = opts.prefix + opts.term](const SubReader& segment,
                                                const term_reader& field,
                                                filter_visitor& visitor) {
        return by_term::visit(segment, field, target, visitor);
      };
    },
    [](const parametric_description& d, const bytes_view prefix,
       const bytes_view term) -> field_visitor {
      struct automaton_context : util::noncopyable {
        automaton_context(const parametric_description& d, bytes_view prefix,
                          bytes_view term)
          : acceptor(make_levenshtein_automaton(d, prefix, term)),
            matcher(MakeAutomatonMatcher(acceptor)) {}

        automaton acceptor;
        automaton_table_matcher matcher;
      };

      auto ctx = std::make_shared<automaton_context>(d, prefix, term);

      if (!Validate(ctx->acceptor)) {
        return [](const SubReader&, const term_reader&, filter_visitor&) {};
      }

      const auto utf8_term_size =
        std::max(1U, static_cast<uint32_t>(utf8_utils::Length(prefix) +
                                           utf8_utils::Length(term)));
      const uint8_t max_distance = d.max_distance() + 1;

      return [ctx = std::move(ctx), utf8_term_size, max_distance](
               const SubReader& segment, const term_reader& field,
               filter_visitor& visitor) mutable {
        return VisitImpl(segment, field, max_distance, utf8_term_size,
                         ctx->matcher, visitor);
      };
    });
}

filter::prepared::ptr by_edit_distance::prepare(
  const PrepareContext& ctx, std::string_view field, bytes_view term,
  size_t scored_terms_limit, uint8_t max_distance, options_type::pdp_f provider,
  bool with_transpositions, bytes_view prefix) {
  return executeLevenshtein(
    max_distance, provider, with_transpositions, prefix, term,
    []() -> prepared::ptr { return prepared::empty(); },
    [&]() -> prepared::ptr {
      if (!prefix.empty() && !term.empty()) {
        bstring target;
        target.reserve(prefix.size() + term.size());
        target += prefix;
        target += term;
        return by_term::prepare(ctx, field, target);
      }

      return by_term::prepare(ctx, field, prefix.empty() ? term : prefix);
    },
    [&, scored_terms_limit](const parametric_description& d,
                            const bytes_view prefix,
                            const bytes_view term) -> prepared::ptr {
      return prepare_levenshtein_filter(ctx, field, prefix, term,
                                        scored_terms_limit, d);
    });
}

}  // namespace irs
