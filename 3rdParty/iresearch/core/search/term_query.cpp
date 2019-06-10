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

#include "shared.hpp"
#include "term_query.hpp"
#include "score_doc_iterators.hpp"

#include "index/index_reader.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                         term_query implementation
// -----------------------------------------------------------------------------

term_query::ptr term_query::make(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term) {
  term_query::states_t states(index.size());
  auto collectors = ord.prepare_collectors(1);

  // iterate over the segments
  for (const auto& segment : index) {
    // get field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    collectors.collect(segment, *reader); // collect field statistics once per segment

    // find term
    auto terms = reader->iterator();

    if (!terms->seek(term)) {
      continue;
    }

    // get term metadata
    auto& meta = terms->attributes().get<term_meta>();

    // read term attributes
    terms->read();

    // Cache term state in prepared query attributes.
    // Later, using cached state we could easily "jump" to
    // postings without relatively expensive FST traversal
    auto& state = states.insert(segment);
    state.reader = reader;
    state.cookie = terms->cookie();

    // collect cost
    if (meta) {
      state.estimation = meta->docs_count;
    }

    collectors.collect(segment, *reader, 0, terms->attributes()); // collect statistics, 0 because only 1 term
  }

  bstring stats(ord.stats_size(), 0);
  collectors.finish(const_cast<byte_type*>(stats.data()), index);

  return memory::make_shared<term_query>(
    std::move(states), std::move(stats), boost
  );
}

term_query::term_query(
    term_query::states_t&& states,
    bstring&& stats,
    boost_t boost)
  : filter::prepared(std::move(stats), boost),
    states_(std::move(states)) {
}

doc_iterator::ptr term_query::execute(
    const sub_reader& rdr,
    const order::prepared& ord,
    const attribute_view& /*ctx*/) const {
  // get term state for the specified reader
  auto state = states_.find(rdr);
  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }

  // find term using cached state
  auto terms = state->reader->iterator();

  // use bytes_ref::blank here since we need just to "jump" to the cached state,
  // and we are not interested in term value itself
  if (!terms->seek(bytes_ref::NIL, *state->cookie)) {
    return doc_iterator::empty();
  }

  // return iterator
  return doc_iterator::make<basic_doc_iterator>(
    rdr, 
    *state->reader,
    stats(),
    terms->postings(ord.features()), 
    ord, 
    state->estimation,
    boost()
  );
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
