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

#include "shared.hpp"
#include "search/term_filter.hpp"
#include "search/limited_sample_collector.hpp"
#include "search/top_terms_collector.hpp"
#include "search/all_terms_collector.hpp"
#include "search/filter_visitor.hpp"
#include "search/multiterm_query.hpp"
#include "index/index_reader.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/levenshtein_default_pdp.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/utf8_utils.hpp"
#include "utils/std.hpp"

namespace {

using namespace irs;

////////////////////////////////////////////////////////////////////////////////
/// @returns levenshtein similarity
////////////////////////////////////////////////////////////////////////////////
FORCE_INLINE boost_t similarity(uint32_t distance, uint32_t size) noexcept {
  assert(size);

  static_assert(sizeof(boost_t) == sizeof(uint32_t),
                "sizeof(boost_t) != sizeof(uint32_t)");

  return 1.f - boost_t(distance) / size;
}

template<typename Invalid, typename Term, typename Levenshtein>
inline auto executeLevenshtein(byte_type max_distance,
                               by_edit_distance_options::pdp_f provider,
                               bool with_transpositions,
                               Invalid inv, Term t, Levenshtein lev) {
  if (!provider) {
    provider = &default_pdp;
  }

  if (0 == max_distance) {
    return t();
  }

  assert(provider);
  const auto& d = (*provider)(max_distance, with_transpositions);

  if (!d) {
    return inv();
  }

  return lev(d);
}

template<typename StatesType>
struct aggregated_stats_visitor : util::noncopyable {
  aggregated_stats_visitor(
      StatesType& states,
      const term_collectors& term_stats) noexcept
    : term_stats(term_stats),
      states(states) {
  }

  void operator()(const irs::sub_reader& segment,
                  const irs::term_reader& field,
                  uint32_t docs_count) const {
    it = field.iterator();
    this->segment = &segment;
    this->field = &field;
    state = &states.insert(segment);
    state->reader = &field;
    state->scored_states_estimation += docs_count;
  }

  void operator()(seek_term_iterator::cookie_ptr& cookie) const {
    assert(it);

    if (!it->seek(irs::bytes_ref::NIL, *cookie)) {
      return;
    }

    assert(segment);
    assert(field);
    term_stats.collect(*segment, *field, 0, *it);
    state->scored_states.emplace_back(std::move(cookie), 0, boost);
  }

  const term_collectors& term_stats;
  StatesType& states;
  mutable seek_term_iterator::ptr it;
  mutable typename StatesType::state_type* state{};
  mutable const sub_reader* segment{};
  mutable const term_reader* field{};
  boost_t boost{ irs::no_boost() };
};

class top_terms_collector : public irs::top_terms_collector<top_term_state<boost_t>> {
 public:
  using base_type = irs::top_terms_collector<top_term_state<boost_t>>;

  top_terms_collector(size_t size, field_collectors& field_stats)
    : base_type(size),
      field_stats_(field_stats) {
  }

  void prepare(const sub_reader& segment,
               const term_reader& field,
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
void visit(
    const sub_reader& segment,
    const term_reader& reader,
    const byte_type no_distance,
    const uint32_t utf8_target_size,
    automaton_table_matcher& matcher,
    Visitor& visitor) {
  assert(fst::kError != matcher.Properties(0));
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

      const auto utf8_value_size = static_cast<uint32_t>(utf8_utils::utf8_length(terms->value()));
      const auto boost = ::similarity(*distance, std::min(utf8_value_size, utf8_target_size));

      visitor.visit(boost);
    } while (terms->next());
  }
}

template<typename Collector>
bool collect_terms(
    const index_reader& index,
    const string_ref& field,
    const bytes_ref& term,
    const parametric_description& d,
    Collector& collector) {
  const auto acceptor = make_levenshtein_automaton(d, term);

  if (!validate(acceptor)) {
    return false;
  }

  auto matcher = make_automaton_matcher(acceptor);
  const uint32_t utf8_term_size = std::max(1U, uint32_t(utf8_utils::utf8_length(term)));
  const byte_type max_distance = d.max_distance() + 1;

  for (auto& segment : index) {
    auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    visit(segment, *reader, max_distance, utf8_term_size, matcher, collector);
  }

  return true;
}

filter::prepared::ptr prepare_levenshtein_filter(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term,
    size_t terms_limit,
    const parametric_description& d) {
  field_collectors field_stats(order);
  term_collectors term_stats(order, 1);
  multiterm_query::states_t states(index.size());

  if (!terms_limit) {
    all_terms_collector<decltype(states)> term_collector(states, field_stats, term_stats);
    term_collector.stat_index(0); // aggregate stats from different terms

    if (!collect_terms(index, field, term, d, term_collector)) {
      return filter::prepared::empty();
    }
  } else {
    top_terms_collector term_collector(terms_limit, field_stats);

    if (!collect_terms(index, field, term, d, term_collector)) {
      return filter::prepared::empty();
    }

    aggregated_stats_visitor<decltype(states)> aggregate_stats(states, term_stats);
    term_collector.visit([&aggregate_stats](top_term_state<boost_t>& state) {
      aggregate_stats.boost = std::max(0.f, state.key);
      state.visit(aggregate_stats);
    });
  }

  std::vector<bstring> stats(1);
  stats.back().resize(order.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats[0].data());
  term_stats.finish(stats_buf, 0, field_stats, index);

  return memory::make_managed<multiterm_query>(
    std::move(states), std::move(stats),
    boost, sort::MergeType::MAX);
}

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                   by_edit_distance implementation
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(by_edit_distance)

/*static*/ field_visitor by_edit_distance::visitor(const options_type::filter_options& opts) {
  return executeLevenshtein(
    opts.max_distance, opts.provider, opts.with_transpositions,
    []() -> field_visitor {
      return [](const sub_reader&, const term_reader&, filter_visitor&){};
    },
    [&opts]() -> field_visitor {
      // must copy term as it may point to temporary string
      return [term = opts.term](
          const sub_reader& segment,
          const term_reader& field,
          filter_visitor& visitor){
        return by_term::visit(segment, field, term, visitor);
      };
    },
    [&opts](const parametric_description& d) -> field_visitor {
      struct automaton_context : util::noncopyable {
        automaton_context(const parametric_description& d, const bytes_ref& term)
          : acceptor(make_levenshtein_automaton(d, term)),
            matcher(make_automaton_matcher(acceptor)) {
        }

        automaton acceptor;
        automaton_table_matcher matcher;
      };

      // FIXME
      auto ctx = memory::make_shared<automaton_context>(d, opts.term);

      if (!validate(ctx->acceptor)) {
        return [](const sub_reader&, const term_reader&, filter_visitor&){};
      }

      const uint32_t utf8_term_size = std::max(1U, uint32_t(utf8_utils::utf8_length(opts.term)));
      const byte_type max_distance = d.max_distance() + 1;

      return [ctx, utf8_term_size, max_distance](
          const sub_reader& segment,
          const term_reader& field,
          filter_visitor& visitor) mutable {
        return ::visit(segment, field, max_distance,
                       utf8_term_size, ctx->matcher, visitor);
      };
    }
  );
}

/*static*/ filter::prepared::ptr by_edit_distance::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term,
    size_t scored_terms_limit,
    byte_type max_distance,
    options_type::pdp_f provider,
    bool with_transpositions) {
  return executeLevenshtein(
    max_distance, provider, with_transpositions,
    []() -> filter::prepared::ptr {
      return prepared::empty();
    },
    [&index, &order, boost, &field, &term]() -> filter::prepared::ptr {
      return by_term::prepare(index, order, boost, field, term);
    },
    [&field, &term, scored_terms_limit, &index, &order, boost](
        const parametric_description& d) -> filter::prepared::ptr {
      return prepare_levenshtein_filter(index, order, boost, field, term, scored_terms_limit, d);
    }
  );
}

}
