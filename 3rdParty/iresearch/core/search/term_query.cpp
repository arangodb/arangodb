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
////////////////////////////////////////////////////////////////////////////////

#include "term_query.hpp"

#include "index/index_reader.hpp"
#include "search/score.hpp"

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                         term_query implementation
// -----------------------------------------------------------------------------

term_query::term_query(
    term_query::states_t&& states,
    bstring&& stats,
    boost_t boost)
  : filter::prepared(boost),
    states_(std::move(states)),
    stats_(std::move(stats)) {
}

doc_iterator::ptr term_query::execute(
    const sub_reader& rdr,
    const order::prepared& ord,
    const attribute_provider* /*ctx*/) const {
  // get term state for the specified reader
  auto state = states_.find(rdr);

  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }

  // find term using cached state
  auto terms = state->reader->iterator();

  if (IRS_UNLIKELY(!terms)) {
    return doc_iterator::empty();
  }

  // use bytes_ref::blank here since we need just to "jump" to the cached state,
  // and we are not interested in term value itself
  if (!terms->seek(bytes_ref::NIL, *state->cookie)) {
    return doc_iterator::empty();
  }

  auto docs = terms->postings(ord.features());
  assert(docs);

  if (!ord.empty()) {
    auto* score = irs::get_mutable<irs::score>(docs.get());

    if (score) {
      order::prepared::scorers scorers(
        ord, rdr, *state->reader,
        stats_.c_str(), score->realloc(ord),
        *docs, boost());

      irs::reset(*score, std::move(scorers));
    }
  }

  return docs;
}

} // ROOT
