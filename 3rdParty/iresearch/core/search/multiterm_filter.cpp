////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "multiterm_filter.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                 all_term_selector
// -----------------------------------------------------------------------------

all_term_selector::all_term_selector(
    const index_reader& index, const order::prepared& order
): term_selector(index, order) {
}

void all_term_selector::build(attribute_store& attrs) {
  collector_.finish(attrs, index_);
}

void all_term_selector::insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
) {

  collector_.collect(segment, field, term.attributes()); // collect statistics
}

// -----------------------------------------------------------------------------
// --SECTION--                               limited_term_selector_by_field_size
// -----------------------------------------------------------------------------

limited_term_selector_by_field_size::limited_term_selector_by_field_size(
    const index_reader& index, const order::prepared& order, size_t limit
): term_selector(index, order), limit_(limit) {
}

void limited_term_selector_by_field_size::build(attribute_store& attrs) {
  // iterate over the scoring candidates
  for (auto& entry: states_) {
    auto& state = entry.second;
    collector_.collect(state.segment, state.field, state.term->attributes()); // collect statistics
  }

  collector_.finish(attrs, index_);
}

void limited_term_selector_by_field_size::insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
) {
  auto terms = field.iterator();

  if (!terms || !terms->seek(term.value())) {
    return; // nothing to do
  }

  if (states_.find(&field) == states_.end()) {
    candidates_.emplace(field.size(), &field);
  }

  states_.emplace(&field, state_t{segment, field, std::move(terms)});

  // if there are too many candidates then remove the least significant
  if (candidates_.size() > limit_) {
    auto itr = candidates_.begin();

    states_.erase(itr->second);
    candidates_.erase(itr);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                            limited_term_selector_by_postings_size
// -----------------------------------------------------------------------------

limited_term_selector_by_postings_size::limited_term_selector_by_postings_size(
    const index_reader& index, const order::prepared& order, size_t limit
): term_selector(index, order), limit_(limit) {
}

void limited_term_selector_by_postings_size::build(attribute_store& attrs) {
  // iterate over the scoring candidates
  for (auto& entry: states_) {
    auto& state = entry.second;
    collector_.collect(state.segment, state.field, state.term->attributes()); // collect statistics
  }

  collector_.finish(attrs, index_);
}

void limited_term_selector_by_postings_size::insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
) {
  auto terms = field.iterator();

  if (!terms || !terms->seek(term.value())) {
    return; // nothing to do
  }

  terms->read(); // update attribute values

  auto& meta = terms->attributes().get<term_meta>();
  auto significance = meta ? meta->docs_count : 0;

  states_.emplace(significance, state_t{segment, field, std::move(terms)});

  // if there are too many candidates then remove the least significant
  if (states_.size() > limit_) {
    states_.erase(states_.begin());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                             limited_term_selector_by_segment_size
// -----------------------------------------------------------------------------

limited_term_selector_by_segment_live_docs::limited_term_selector_by_segment_live_docs(
    const index_reader& index, const order::prepared& order, size_t limit
): term_selector(index, order), limit_(limit) {
}

void limited_term_selector_by_segment_live_docs::build(attribute_store& attrs) {
  // iterate over the scoring candidates
  for (auto& entry: states_) {
    auto& state = entry.second;
    collector_.collect(state.segment, state.field, state.term->attributes()); // collect statistics
  }

  collector_.finish(attrs, index_);
}

void limited_term_selector_by_segment_live_docs::insert(
    const sub_reader& segment,
    const term_reader& field,
    const term_iterator& term
) {
  auto terms = field.iterator();

  if (!terms || !terms->seek(term.value())) {
    return; // nothing to do
  }

  if (states_.find(&segment) == states_.end()) {
    candidates_.emplace(segment.live_docs_count(), &segment);
  }

  states_.emplace(&segment, state_t{segment, field, std::move(terms)});

  // if there are too many candidates then remove the least significant
  if (candidates_.size() > limit_) {
    auto itr = candidates_.begin();

    states_.erase(itr->second);
    candidates_.erase(itr);
  }
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------