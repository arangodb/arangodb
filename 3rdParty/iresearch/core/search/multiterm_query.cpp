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

NS_ROOT

doc_iterator::ptr multiterm_query::execute(
    const sub_reader& segment,
    const order::prepared& ord,
    const attribute_view& /*ctx*/) const {
  typedef disjunction<doc_iterator::ptr> disjunction_t;

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
    itrs.emplace_back(doc_iterator::make<bitset_doc_iterator>(
      state->unscored_docs
    ));
  }

  auto& stats = this->stats();

  // add an iterator for each of the scored states
  for (auto& entry : state->scored_states) {
    assert(entry.first);
    if (!terms->seek(bytes_ref::NIL, *entry.first)) {
      return doc_iterator::empty(); // internal error
    }

    auto docs = terms->postings(features);
    auto& attrs = docs->attributes();

    // set score
    auto& score = attrs.get<irs::score>();
    if (score) {
      assert(entry.second < stats.size());
      auto* stat = stats[entry.second].c_str();

      score->prepare(ord, ord.prepare_scorers(segment, *state->reader, stat, attrs, boost()));
    }

    itrs.emplace_back(std::move(docs));

    if (IRS_UNLIKELY(!itrs.back().it)) {
      itrs.pop_back();
      continue;
    }
  }

  return make_disjunction<disjunction_t>(std::move(itrs), ord, state->estimation);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
