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

#include "limited_sample_scorer.hpp"

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "utils/hash_utils.hpp"

NS_LOCAL

struct state {
  explicit state(const irs::index_reader& index,
                   const irs::term_reader& field,
                   const irs::order::prepared& order,
                   size_t& state_offset)
    : collectors(order.prepare_collectors(1)) { // 1 term per bstring because a range is treated as a disjunction

    // once per every 'state' collect field statistics over the entire index
    for (auto& segment: index) {
      collectors.collect(segment, field); // collect field statistics once per segment
    }

    stats_offset = state_offset++;
  }

  irs::order::prepared::collectors collectors;
  size_t stats_offset;
};

void set_doc_ids(irs::bitset& buf, const irs::term_iterator& term, size_t docs_count) {
  docs_count += (irs::doc_limits::min)();
  if (buf.size() < docs_count) {
    buf.reset(docs_count); // ensure we have enough space
  }

  auto itr = term.postings(irs::flags::empty_instance());

  if (!itr) {
    return; // no doc_ids in iterator
  }

//FIXME use doc attribute
//  auto* doc = itr->attributes().get<irs::document>().get();
//
//  if (!doc) {
//    return; // no doc value
//  }

  while (itr->next()) {
    buf.set(itr->value());
  }
};

bool less(size_t lhs_priority, size_t rhs_priority,
          size_t lhs_state_offset, size_t rhs_state_offset) noexcept {
  return rhs_priority < lhs_priority
      || (rhs_priority == lhs_priority && lhs_state_offset < rhs_state_offset);
}

NS_END

NS_ROOT

limited_sample_scorer::limited_sample_scorer(size_t scored_terms_limit)
  : scored_terms_limit_(scored_terms_limit) {
  scored_states_.reserve(scored_terms_limit);
}

void limited_sample_scorer::score(const index_reader& index,
                                  const order::prepared& order,
                                  std::vector<bstring>& stats) {
  if (!scored_terms_limit_) {
    return; // nothing to score (optimization)
  }

  // stats for a specific term
  std::unordered_map<hashed_bytes_ref, state> term_stats;

  // iterate over all the states from which statistcis should be collected
  size_t stats_offset = 0;
  for (auto& scored_state : scored_states_) {
    assert(scored_state.cookie);
    auto& field = *scored_state.state->reader;
    auto term_itr = field.iterator(); // FIXME
    assert(term_itr);

    // find the stats for the current term
    const auto res = map_utils::try_emplace(
      term_stats,
      make_hashed_ref(bytes_ref(scored_state.term), std::hash<bytes_ref>()),
      index, field, order, stats_offset);

    // find term attributes using cached state
    if (!term_itr->seek(bytes_ref::NIL, *(scored_state.cookie))) {
      continue; // some internal error that caused the term to disapear
    }

    auto& stats_entry = res.first->second;

    // collect statistics, 0 because only 1 term
    stats_entry.collectors.collect(*scored_state.segment, field, 0, term_itr->attributes());

    scored_state.state->scored_states.emplace_back(
      std::move(scored_state.cookie),
      stats_entry.stats_offset);
  }

  // iterate over all stats and apply/store order stats
  stats.resize(stats_offset);
  for (auto& entry : term_stats) {
    auto& stats_entry = stats[entry.second.stats_offset];
    stats_entry.resize(order.stats_size());
    auto* stats_buf = const_cast<byte_type*>(stats_entry.data());

    order.prepare_stats(stats_buf);
    entry.second.collectors.finish(stats_buf, index);
  }
}

void limited_sample_scorer::collect(
    size_t priority,
    size_t scored_state_id,
    limited_sample_state& scored_state,
    const sub_reader& segment,
    const seek_term_iterator& term_itr) {
  if (!scored_terms_limit_) {
    // state will not be scored
    // add all doc_ids from the doc_iterator to the unscored_docs
    set_doc_ids(scored_state.unscored_docs, term_itr, segment.docs_count());

    return; // nothing to collect (optimization)
  }

  auto less = [](const scored_term_state& lhs,
                 const scored_term_state& rhs) noexcept {
    return ::less(lhs.priority, rhs.priority, lhs.state_offset, rhs.state_offset);
  };

  if (scored_states_.size() < scored_terms_limit_) {
    // have not reached the scored state limit yet
    scored_states_.emplace_back(priority, segment, scored_state, scored_state_id, term_itr.value(), term_itr.cookie());

    std::push_heap(scored_states_.begin(), scored_states_.end(), less);
  } else if (::less(priority, scored_states_.front().priority, scored_states_.front().state_offset, scored_state_id)) { // FIXME
    std::pop_heap(scored_states_.begin(), scored_states_.end(), less);

    auto& state = scored_states_.back();
    auto state_term_it = state.state->reader->iterator(); // FIXME cache iterator???

    assert(state.cookie);
    if (state_term_it->seek(bytes_ref::NIL, *state.cookie)) {
      // state will not be scored
      // add all doc_ids from the doc_iterator to the unscored_docs
      set_doc_ids(state.state->unscored_docs, *state_term_it, state.segment->docs_count());
    }

    // update min state
    state.priority = priority;
    state.state = &scored_state;
    state.cookie = term_itr.cookie();
    state.term = term_itr.value();
    state.segment = &segment;
    state.state_offset = scored_state_id;

    std::push_heap(scored_states_.begin(), scored_states_.end(), less);
  } else {
    // state will not be scored
    // add all doc_ids from the doc_iterator to the unscored_docs
    set_doc_ids(scored_state.unscored_docs, term_itr, segment.docs_count());
  }
}

NS_END
