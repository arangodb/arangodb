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

#ifndef IRESEARCH_RANGE_QUERY_H
#define IRESEARCH_RANGE_QUERY_H

#include <map>
#include <unordered_map>

#include "filter.hpp"
#include "cost.hpp"
#include "utils/bitset.hpp"
#include "utils/string.hpp"

NS_ROOT

struct term_reader;

//////////////////////////////////////////////////////////////////////////////
/// @class range_state
/// @brief cached per reader range state
//////////////////////////////////////////////////////////////////////////////
struct range_state : private util::noncopyable {
  range_state() = default;

  range_state(range_state&& rhs) NOEXCEPT {
    *this = std::move(rhs);
  }

  range_state& operator=(range_state&& other) NOEXCEPT {
    if (this == &other) {
      return *this;
    }

    reader = std::move(other.reader);
    min_term = std::move(other.min_term);
    min_cookie = std::move(other.min_cookie);
    estimation = std::move(other.estimation);
    count = std::move(other.count);
    scored_states = std::move(other.scored_states);
    unscored_docs = std::move(other.unscored_docs);
    other.reader = nullptr;
    other.count = 0;
    other.estimation = 0;

    return *this;
  }

  const term_reader* reader{}; // reader using for iterate over the terms
  bstring min_term; // minimum term to start from
  seek_term_iterator::seek_cookie::ptr min_cookie; // cookie corresponding to the start term
  cost::cost_t estimation{}; // per-segment query estimation
  size_t count{}; // number of terms to process from start term

  // scored states/stats by their offset in range_state (i.e. offset from min_term)
  // range_query::execute(...) expects an orderd map
  std::map<size_t, attribute_store> scored_states;

  // matching doc_ids that may have been skipped while collecting statistics and should not be scored by the disjunction
  bitset unscored_docs;
}; // reader_state

//////////////////////////////////////////////////////////////////////////////
/// @brief object to collect and track a limited number of scorers
//////////////////////////////////////////////////////////////////////////////
class limited_sample_scorer {
 public:
  limited_sample_scorer(size_t scored_terms_limit);
  void collect(
    size_t priority, // priority of this entry, lowest priority removed first
    size_t scored_state_id, // state identifier used for querying of attributes
    iresearch::range_state& scored_state, // state containing this scored term
    const iresearch::sub_reader& reader, // segment reader for the current term
    const seek_term_iterator& term_itr // term-iterator positioned at the current term
  );
  void score(const index_reader& index, const order::prepared& order);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief a representation of a term cookie with its asociated range_state
  //////////////////////////////////////////////////////////////////////////////
  struct scored_term_state_t {
    seek_term_iterator::cookie_ptr cookie; // term offset cache
    iresearch::range_state& state; // state containing this scored term
    size_t state_offset;
    const iresearch::sub_reader& sub_reader; // segment reader for the current term
    bstring term; // actual term value this state is for

    scored_term_state_t(
      const iresearch::sub_reader& sr,
      iresearch::range_state& scored_state,
      size_t scored_state_offset,
      const seek_term_iterator& term_itr
    ):
      cookie(term_itr.cookie()),
      state(scored_state),
      state_offset(scored_state_offset),
      sub_reader(sr),
      term(term_itr.value()) {
    }
  };

  typedef std::multimap<size_t, scored_term_state_t> scored_term_states_t;
  scored_term_states_t scored_states_;
  size_t scored_terms_limit_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class range_query
/// @brief compiled query suitable for filters with continious range of terms
///        like "by_range" or "by_prefix". 
//////////////////////////////////////////////////////////////////////////////
class range_query : public filter::prepared {
 public:
  typedef states_cache<range_state> states_t;

  DECLARE_SHARED_PTR(range_query);

  explicit range_query(states_t&& states);

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const override;

 private:
  states_t states_;
}; // range_query 

NS_END // ROOT

#endif // IRESEARCH_RANGE_QUERY_H 
