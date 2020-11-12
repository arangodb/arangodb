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

#ifndef IRESEARCH_ALL_TERMS_COLLECTOR_H
#define IRESEARCH_ALL_TERMS_COLLECTOR_H

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "search/sort.hpp"
#include "search/collectors.hpp"
#include "utils/noncopyable.hpp"

namespace iresearch {

template<typename States>
class all_terms_collector : util::noncopyable {
 public:
  all_terms_collector(
      States& states,
      field_collectors& field_stats,
      term_collectors& term_stats) noexcept
    : states_(states),
      field_stats_(field_stats),
      term_stats_(term_stats) {
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepare collector for terms collecting
  /// @param segment segment reader for the current term
  /// @param state state containing this scored term
  /// @param terms segment term-iterator positioned at the current term
  //////////////////////////////////////////////////////////////////////////////
  void prepare(const sub_reader& segment,
               const term_reader& field,
               const seek_term_iterator& terms) noexcept {
    field_stats_.collect(segment, field);

    auto& state = states_.insert(segment);
    state.reader = &field;

    state_.state = &state;
    state_.segment = &segment;
    state_.field = &field;
    state_.terms = &terms;

    // get term metadata
    auto* meta = irs::get<term_meta>(terms);
    state_.docs_count = meta ? &meta->docs_count : &no_docs_;
  }

  void visit(const boost_t boost) {
    assert(state_);
    term_stats_.collect(*state_.segment, *state_.state->reader, stat_index_, *state_.terms);

    auto& state = *state_.state;
    state.scored_states.emplace_back(state_.terms->cookie(), stat_index_, boost);
    state.scored_states_estimation += *state_.docs_count;
  }

  uint32_t stat_index() const noexcept { return stat_index_; }
  void stat_index(uint32_t stat_index) noexcept { stat_index_ = stat_index; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief a representation of state of the collector
  //////////////////////////////////////////////////////////////////////////////
  struct collector_state {
    const sub_reader* segment{};
    const term_reader* field{};
    const seek_term_iterator* terms{};
    const uint32_t* docs_count{};
    typename States::state_type* state{};

    operator bool() const noexcept {
      return segment && field && terms && state;
    }
  };

  collector_state state_;
  States& states_;
  field_collectors& field_stats_;
  term_collectors& term_stats_;
  uint32_t stat_index_{};
  const decltype(term_meta::docs_count) no_docs_{0};
};

}

#endif // IRESEARCH_ALL_TERMS_COLLECTOR_H

