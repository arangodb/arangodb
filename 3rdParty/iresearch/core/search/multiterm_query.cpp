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

#include "multiterm_query.hpp"

#include "shared.hpp"
#include "bitset_doc_iterator.hpp"
#include "disjunction.hpp"

namespace iresearch {

doc_iterator::ptr multiterm_query::execute(
    const sub_reader& segment,
    const order::prepared& ord,
    const attribute_provider* /*ctx*/) const {
  using scored_disjunction_t = scored_disjunction_iterator<doc_iterator::ptr>;
  using disjunction_t = disjunction_iterator<doc_iterator::ptr>;

  // get term state for the specified reader
  auto state = states_.find(segment);

  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }

  // get terms iterator
  auto terms = state->reader->iterator();

  if (IRS_UNLIKELY(!terms)) {
    return doc_iterator::empty();
  }

  // prepared disjunction
  const bool has_bit_set = state->unscored_docs.any();
  disjunction_t::doc_iterators_t itrs;
  itrs.reserve(state->scored_states.size() + size_t(has_bit_set)); // +1 for possible bitset_doc_iterator

  // get required features for order
  auto& features = ord.features();

  // add an iterator for the unscored docs
  if (has_bit_set) {
    // ensure first bit isn't set,
    // since we don't want to emit doc_limits::invalid()
    assert(state->unscored_docs.any() && !state->unscored_docs.test(0));

    itrs.emplace_back(memory::make_managed<bitset_doc_iterator>(state->unscored_docs));
  }

  auto& stats = this->stats();

  // add an iterator for each of the scored states
  const bool no_score = ord.empty();
  for (auto& entry : state->scored_states) {
    assert(entry.cookie);
    if (!terms->seek(bytes_ref::NIL, *entry.cookie)) {
      return doc_iterator::empty(); // internal error
    }

    auto docs = terms->postings(features);

    if (!no_score) {
      auto* score = irs::get_mutable<irs::score>(docs.get());

      if (score) {
        assert(entry.stat_offset < stats.size());
        auto* stat = stats[entry.stat_offset].c_str();

        order::prepared::scorers scorers(
          ord, segment, *state->reader, stat,
          score->realloc(ord), *docs, entry.boost*boost());

        irs::reset(*score, std::move(scorers));
      }
    }

    itrs.emplace_back(std::move(docs));

    if (IRS_UNLIKELY(!itrs.back().it)) {
      itrs.pop_back();
      continue;
    }
  }

  if (ord.empty()) {
    return make_disjunction<disjunction_t>(
      std::move(itrs), ord, merge_type_, state->estimation());
  }

  return make_disjunction<scored_disjunction_t>(
    std::move(itrs), ord, merge_type_, state->estimation());
}

} // ROOT
