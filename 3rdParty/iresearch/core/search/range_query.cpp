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

#include "bitset_doc_iterator.hpp"
#include "shared.hpp"
#include "range_query.hpp"
#include "disjunction.hpp"
#include "score_doc_iterators.hpp"
#include "index/index_reader.hpp"
#include "utils/hash_utils.hpp"

NS_LOCAL

void set_doc_ids(irs::bitset& buf, const irs::term_iterator& term) {
  auto itr = term.postings(irs::flags::empty_instance());

  if (!itr) {
    return; // no doc_ids in iterator
  }

  while(itr->next()) {
    buf.set(itr->value());
  }
}

NS_END

NS_ROOT

limited_sample_scorer::limited_sample_scorer(size_t scored_terms_limit):
  scored_terms_limit_(scored_terms_limit) {
}

void limited_sample_scorer::collect(
  size_t priority, // priority of this entry, lowest priority removed first
  size_t scored_state_id, // state identifier used for querying of attributes
  range_state& scored_state, // state containing this scored term
  const sub_reader& reader, // segment reader for the current term
  const seek_term_iterator& term_itr // term-iterator positioned at the current term
) {
  if (!scored_terms_limit_) {
    assert(scored_state.unscored_docs.size() >= (doc_limits::min)() + reader.docs_count()); // otherwise set will fail
    set_doc_ids(scored_state.unscored_docs, term_itr);

    return; // nothing to collect (optimization)
  }

  scored_states_.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(priority),
    std::forward_as_tuple(reader, scored_state, scored_state_id, term_itr)
  );

  if (scored_states_.size() <= scored_terms_limit_) {
    return; // have not reached the scored state limit yet
  }

  auto itr = scored_states_.begin(); // least significant state to be removed
  auto& entry = itr->second;
  auto state_term_itr = entry.state.reader->iterator();

  // add all doc_ids from the doc_iterator to the unscored_docs
  if (state_term_itr
      && entry.cookie
      && state_term_itr->seek(bytes_ref::NIL, *(entry.cookie))) {
    assert(entry.state.unscored_docs.size() >= (doc_limits::min)() + entry.sub_reader.docs_count()); // otherwise set will fail
    set_doc_ids(entry.state.unscored_docs, *state_term_itr);
  }

  scored_states_.erase(itr);
}

void limited_sample_scorer::score(
    const index_reader& index, const order::prepared& order
) {
  if (!scored_terms_limit_) {
    return; // nothing to score (optimization)
  }

  struct state_t {
    order::prepared::collectors collectors;
    bstring stats; // filter stats for a the current state/term
    state_t(const order::prepared& order)
      : collectors(order.prepare_collectors(1)) { // 1 term per bstring because a range is treated as a disjunction
    }
  };
  std::unordered_map<hashed_bytes_ref, state_t> term_stats; // stats for a specific term
  std::unordered_map<scored_term_state_t*, state_t*> state_stats; // stats for a specific state

  // iterate over all the states from which statistcis should be collected
  for (auto& entry: scored_states_) {
    auto& scored_state = entry.second;
    auto term_itr = scored_state.state.reader->iterator();

    // find the stats for the current term
    auto res = map_utils::try_emplace(
      term_stats,
      make_hashed_ref(bytes_ref(scored_state.term), std::hash<irs::bytes_ref>()),
      order
    );
    auto inserted = res.second;
    auto& stats_entry = res.first->second;
    auto& collectors = stats_entry.collectors;
    auto& field = *scored_state.state.reader;
    auto& segment = scored_state.sub_reader;

    // once per every 'state_t' collect field statistics over the entire index
    if (inserted) {
      for (auto& sr: index) {
        collectors.collect(sr, field); // collect field statistics once per segment
      }
    }

    // find term attributes using cached state
    // use bytes_ref::blank here since we just "jump" to cached state,
    // and we are not interested in term value itself
    if (!term_itr
        || !scored_state.cookie
        || !term_itr->seek(bytes_ref::NIL, *(scored_state.cookie))) {
      continue; // some internal error that caused the term to disapear
    }

    collectors.collect(segment, field, 0, term_itr->attributes()); // collect statistics, 0 because only 1 term
    state_stats.emplace(&scored_state, &stats_entry); // associate states to a state
  }

  // iterate over all stats and apply/store order stats
  for (auto& entry: term_stats) {
    entry.second.stats.resize(order.stats_size());
    entry.second.collectors.finish(
      const_cast<byte_type*>(entry.second.stats.data()),
      index
    );
  }

  // set filter attributes for each corresponding term
  for (auto& entry: scored_states_) {
    auto& scored_state = entry.second;
    auto itr = state_stats.find(&scored_state);
    assert(itr != state_stats.end() && itr->second); // values set just above

    // filter stats is copied since it's shared among multiple states
    scored_state.state.scored_states.emplace(
      scored_state.state_offset,
      itr->second->stats
    );
  }
}

doc_iterator::ptr range_query::execute(
    const sub_reader& rdr,
    const order::prepared& ord,
    const attribute_view& /*ctx*/) const {
  // get term state for the specified reader
  auto state = states_.find(rdr);
  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }

  // get terms iterator
  auto terms = state->reader->iterator();

  // find min term using cached state
  if (!terms->seek(state->min_term, *(state->min_cookie))) {
    return doc_iterator::empty();
  }

  // prepared disjunction
  const bool has_bit_set = state->unscored_docs.any();
  disjunction::doc_iterators_t itrs;
  itrs.reserve(state->count + size_t(has_bit_set)); // +1 for possible bitset_doc_iterator

  // get required features for order
  auto& features = ord.features();

  // add an iterator for the unscored docs
  if (has_bit_set) {
    itrs.emplace_back(doc_iterator::make<bitset_doc_iterator>(
      state->unscored_docs
    ));
  }

  size_t last_offset = 0;

  // add an iterator for each of the scored states
  for (auto& entry: state->scored_states) {
    auto offset = entry.first;
    auto* stats = entry.second.c_str();
    assert(offset >= last_offset);

    if (!skip(*terms, offset - last_offset)) {
      continue; // reached end of iterator
    }

    last_offset = offset;
    itrs.emplace_back(doc_iterator::make<basic_doc_iterator>(
      rdr,
      *state->reader,
      stats,
      terms->postings(features),
      ord,
      state->estimation,
      boost()
    ));
  }

  return make_disjunction<irs::disjunction>(
    std::move(itrs), ord, state->estimation
  );
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
