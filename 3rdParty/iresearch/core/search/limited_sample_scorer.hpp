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

#ifndef IRESEARCH_LIMITED_SAMPLE_SCORER_H
#define IRESEARCH_LIMITED_SAMPLE_SCORER_H

#include "shared.hpp"
#include "sort.hpp"
#include "index/iterators.hpp"
#include "utils/string.hpp"
#include "utils/bitset.hpp"

NS_ROOT

struct sub_reader;
struct index_reader;

//////////////////////////////////////////////////////////////////////////////
/// @struct limited_sample_state
//////////////////////////////////////////////////////////////////////////////
struct limited_sample_state {
  limited_sample_state() = default;
  limited_sample_state(limited_sample_state&& rhs) noexcept
    : reader(rhs.reader),
      scored_states(std::move(rhs.scored_states)),
      unscored_docs(std::move(rhs.unscored_docs)) {
    rhs.reader = nullptr;
  }
  limited_sample_state& operator=(limited_sample_state&& rhs) noexcept {
    if (this != &rhs) {
      scored_states = std::move(rhs.scored_states);
      unscored_docs = std::move(rhs.unscored_docs);
      reader = rhs.reader;
      rhs.reader = nullptr;
    }
    return *this;
  }

  // reader using for iterate over the terms
  const term_reader* reader{};

  // scored term states (cookie + stat offset)
  std::vector<std::pair<seek_term_iterator::seek_cookie::ptr, size_t>> scored_states;

  // matching doc_ids that may have been skipped
  // while collecting statistics and should not be scored by the disjunction
  bitset unscored_docs;
}; // limited_sample_state

//////////////////////////////////////////////////////////////////////////////
/// @class limited_sample_score
/// @brief object to collect and track a limited number of scorers
//////////////////////////////////////////////////////////////////////////////
class limited_sample_scorer : util::noncopyable {
 public:
  explicit limited_sample_scorer(size_t scored_terms_limit);

  //////////////////////////////////////////////////////////////////////////////
  /// @param priority priority of this entry, lowest priority removed first
  /// @param scored_state_id state identifier used for querying of attributes
  /// @param scored_state state containing this scored term
  /// @param reader segment reader for the current term
  /// @param term_itr segment term-iterator positioned at the current term
  //////////////////////////////////////////////////////////////////////////////
  void collect(size_t priority,
               size_t scored_state_id,
               limited_sample_state& scored_state,
               const sub_reader& reader,
               const seek_term_iterator& term_itr);

  void score(const index_reader& index,
             const order::prepared& order,
             std::vector<bstring>& stats);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief a representation of a term cookie with its associated range_state
  //////////////////////////////////////////////////////////////////////////////
  struct scored_term_state {
    scored_term_state(
        size_t priority,
        const sub_reader& segment,
        limited_sample_state& scored_state,
        size_t scored_state_offset,
        const seek_term_iterator& term)
      : cookie(term.cookie()),
        state(&scored_state),
        state_offset(scored_state_offset),
        priority(priority),
        segment(&segment),
        term(term.value()) {
      assert(this->cookie);
    }

    seek_term_iterator::cookie_ptr cookie; // term offset cache
    limited_sample_state* state; // state containing this scored term
    size_t state_offset;
    size_t priority;
    const irs::sub_reader* segment; // segment reader for the current term
    bstring term; // actual term value this state is for
  }; // scored_term_state_t

  typedef std::multimap<size_t, scored_term_state> scored_term_states_t;

  scored_term_states_t scored_states_;
  size_t scored_terms_limit_;
}; // limited_sample_scorer

NS_END

#endif // IRESEARCH_LIMITED_SAMPLE_SCORER_H
