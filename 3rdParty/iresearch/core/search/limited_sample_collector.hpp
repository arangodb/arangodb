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

#ifndef IRESEARCH_LIMITED_SAMPLE_COLLECTOR_H
#define IRESEARCH_LIMITED_SAMPLE_COLLECTOR_H

#include <absl/container/flat_hash_map.h>

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "search/collectors.hpp"
#include "search/filter_visitor.hpp"
#include "search/multiterm_query.hpp"
#include "index/index_reader.hpp"
#include "index/iterators.hpp"
#include "utils/hash_utils.hpp"
#include "utils/string.hpp"

namespace iresearch {

struct sub_reader;
struct index_reader;

//////////////////////////////////////////////////////////////////////////////
/// @class limited_sample_collector
/// @brief object to collect and track a limited number of scorers,
///        terms with longer postings are treated as more important
//////////////////////////////////////////////////////////////////////////////
template<typename Key, typename Comparer = std::less<Key>>
class limited_sample_collector : private irs::compact<0, Comparer>,
                                 private util::noncopyable {
 public:
  using key_type = Key;
  using comparer_type = Comparer;

 private:
  using comparer_rep = irs::compact<0, comparer_type>;

 public:
  explicit limited_sample_collector(size_t scored_terms_limit,
                                    const comparer_type& comparer = {})
    : comparer_rep(comparer),
      scored_terms_limit_(scored_terms_limit) {
      scored_states_.reserve(scored_terms_limit);
      scored_states_heap_.reserve(scored_terms_limit);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepare scorer for terms collecting
  /// @param segment segment reader for the current term
  /// @param state state containing this scored term
  /// @param terms segment term-iterator positioned at the current term
  //////////////////////////////////////////////////////////////////////////////
  void prepare(const sub_reader& segment,
               const seek_term_iterator& terms,
               multiterm_state& scored_state) noexcept {
    state_.state = &scored_state;
    state_.segment = &segment;
    state_.terms = &terms;

    // get term metadata
    auto* meta = irs::get<term_meta>(terms);
    state_.docs_count = meta ? &meta->docs_count : &no_docs_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief collect current term
  //////////////////////////////////////////////////////////////////////////////
  void collect(const Key& key) {
    assert(state_.segment && state_.terms && state_.state);

    if (!scored_terms_limit_) {
      // state will not be scored

      state_.state->unscored_terms.emplace_back(state_.terms->cookie());
      state_.state->unscored_states_estimation += *state_.docs_count;
      return; // nothing to collect (optimization)
    }

    if (scored_states_.size() < scored_terms_limit_) {
      // have not reached the scored state limit yet
      scored_states_heap_.emplace_back(scored_states_.size());
      scored_states_.emplace_back(key, state_);

      push();
      return;
    }

    const size_t min_state_idx = scored_states_heap_.front();

    if (scored_states_[min_state_idx].key < key) {
      pop();

      auto& min_state = scored_states_[min_state_idx];

      assert(min_state.cookie);
      // state will not be scored
      min_state.state->unscored_terms.emplace_back(std::move(min_state.cookie));
      min_state.state->unscored_states_estimation += min_state.docs_count;

      // update min state
      min_state.docs_count = *state_.docs_count;
      min_state.state = state_.state;
      min_state.cookie = state_.terms->cookie();
      min_state.term = state_.terms->value();
      min_state.segment = state_.segment;
      min_state.key = key;

      push();
    } else {
      // state will not be scored
      state_.state->unscored_terms.emplace_back(state_.terms->cookie());
      state_.state->unscored_states_estimation += *state_.docs_count;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finish collecting and evaluate stats
  //////////////////////////////////////////////////////////////////////////////
  void score(const index_reader& index,
             const order::prepared& order,
             std::vector<bstring>& stats) {
    if (!scored_terms_limit_) {
      return; // nothing to score (optimization)
    }

    // stats for a specific term
    absl::flat_hash_map<hashed_bytes_ref, stats_state> term_stats;

    // iterate over all the states from which statistcis should be collected
    uint32_t stats_offset = 0;
    for (auto& scored_state : scored_states_) {
      assert(scored_state.cookie);
      auto& field = *scored_state.state->reader;
      auto term_itr = field.iterator(); // FIXME
      assert(term_itr);

      // find the stats for the current term
      const auto res = term_stats.try_emplace(
        make_hashed_ref(bytes_ref(scored_state.term)),
        index, field, order, stats_offset);

      // find term attributes using cached state
      if (!term_itr->seek(bytes_ref::NIL, *(scored_state.cookie))) {
        continue; // some internal error that caused the term to disappear
      }

      auto& stats_entry = res.first->second;

      // collect statistics, 0 because only 1 term
      stats_entry.term_stats.collect(*scored_state.segment, field, 0, *term_itr);

      scored_state.state->scored_states.emplace_back(
        std::move(scored_state.cookie),
        stats_entry.stats_offset,
        static_cast<boost_t>(scored_state.key));

      // update estimation for scored state
      scored_state.state->scored_states_estimation += scored_state.docs_count;
    }

    // iterate over all stats and apply/store order stats
    stats.resize(stats_offset);
    for (auto& entry : term_stats) {
      auto& stats_entry = stats[entry.second.stats_offset];
      stats_entry.resize(order.stats_size());
      auto* stats_buf = const_cast<byte_type*>(stats_entry.data());

      entry.second.term_stats.finish(stats_buf, 0, entry.second.field_stats, index);
    }
  }

 private:
  struct stats_state {
    explicit stats_state(
        const irs::index_reader& index,
        const irs::term_reader& field,
        const irs::order::prepared& order,
        uint32_t& state_offset)
      : field_stats(order),
        term_stats(order, 1) { // 1 term per bstring because a range is treated as a disjunction

      // once per every 'state' collect field statistics over the entire index
      for (auto& segment: index) {
        // FIXME
        field_stats.collect(segment, field); // collect field statistics once per segment
      }

      stats_offset = state_offset++;
    }

    field_collectors field_stats;
    term_collectors term_stats;
    uint32_t stats_offset;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a representation of state of the collector
  //////////////////////////////////////////////////////////////////////////////
  struct collector_state {
    const sub_reader* segment{};
    const seek_term_iterator* terms{};
    multiterm_state* state{};
    const uint32_t* docs_count{};
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a representation of a term cookie with its associated range_state
  //////////////////////////////////////////////////////////////////////////////
  struct scored_term_state {
    scored_term_state(const Key& key, const collector_state& state)
      : key(key),
        cookie(state.terms->cookie()),
        state(state.state),
        segment(state.segment),
        term(state.terms->value()),
        docs_count(*state.docs_count) {
      assert(this->cookie);
    }

    scored_term_state(scored_term_state&&) = default;
    scored_term_state& operator=(scored_term_state&&) = default;

    Key key;
    seek_term_iterator::cookie_ptr cookie; // term offset cache
    multiterm_state* state; // state containing this scored term
    const irs::sub_reader* segment; // segment reader for the current term
    bstring term; // actual term value this state is for
    uint32_t docs_count;
  };

  void push() noexcept {
    std::push_heap(
      scored_states_heap_.begin(),
      scored_states_heap_.end(),
      [this](const size_t lhs, const size_t rhs) noexcept {
        return comparer()(scored_states_[rhs].key, scored_states_[lhs].key);
    });
  }

  void pop() noexcept {
    std::pop_heap(
      scored_states_heap_.begin(),
      scored_states_heap_.end(),
      [this](const size_t lhs, const size_t rhs) noexcept {
        return comparer()(scored_states_[rhs].key, scored_states_[lhs].key);
    });
  }

  const comparer_type& comparer() const noexcept {
    return comparer_rep::get();
  }

  const decltype(term_meta::docs_count) no_docs_ = 0;
  collector_state state_;
  std::vector<scored_term_state> scored_states_;
  std::vector<size_t> scored_states_heap_; // use external heap as states are big
  size_t scored_terms_limit_;
}; // limited_sample_collector

struct term_frequency {
  uint32_t offset;
  uint32_t frequency;
  boost_t boost;

  explicit operator boost_t() const noexcept {
    return boost;
  }

  bool operator<(const term_frequency& rhs) const noexcept {
    return frequency < rhs.frequency
        || (frequency == rhs.frequency && offset < rhs.offset);
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class multiterm_visitor
/// @brief filter visitor for multiterm queries
//////////////////////////////////////////////////////////////////////////////
template<typename States>
class multiterm_visitor {
 public:
  multiterm_visitor(
      limited_sample_collector<term_frequency>& collector,
      States& states)
    : collector_(collector), states_(states) {
  }

  void prepare(
      const sub_reader& segment,
      const term_reader& reader,
      const seek_term_iterator& terms) {
    // get term metadata
    auto* meta = irs::get<term_meta>(terms);

    // NOTE: we can't use reference to 'docs_count' here, like
    // 'const auto& docs_count = meta ? meta->docs_count : NO_DOCS;'
    // since not gcc4.9 nor msvc2015-2019 can handle this correctly
    // probably due to broken optimization
    docs_count_ = meta ? &meta->docs_count : &no_docs_;

    // get state for current segment
    auto& state = states_.insert(segment);
    state.reader = &reader;

    collector_.prepare(segment, terms, state);
    key_.offset = 0;
  }

  // FIXME can incorporate boost into collecting logic
  void visit(boost_t boost) {
    // fill scoring candidates
    assert(docs_count_);
    key_.frequency = *docs_count_;
    key_.boost = boost;
    collector_.collect(key_);
    ++key_.offset;
  }

 private:
  const decltype(term_meta::docs_count) no_docs_ = 0;
  limited_sample_collector<term_frequency>& collector_;
  States& states_;
  term_frequency key_;
  const decltype(term_meta::docs_count)* docs_count_ = nullptr;
}; // multiterm_visitor

}

#endif // IRESEARCH_LIMITED_SAMPLE_COLLECTOR_H
