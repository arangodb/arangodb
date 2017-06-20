//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "term_query.hpp"
#include "score_doc_iterators.hpp"

#include "index/index_reader.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                         term_query implementation
// -----------------------------------------------------------------------------

term_query::term_query(term_query::states_t&& states, attribute_store&& attrs)
  : filter::prepared(std::move(attrs)), states_(std::move(states)) {
}

score_doc_iterator::ptr term_query::execute(
    const sub_reader& rdr,
    const order::prepared& ord) const {
  /* get term state for the specified reader */
  auto state = states_.find(rdr);
  if (!state) {
    /* invalid state */
    return score_doc_iterator::empty();
  }

  /* find term using cached state */
  auto terms = state->reader->iterator();

  /* use bytes_ref::nil here since we need just to "jump" to the cached state,
   * and we are not interested in term value itself */
  if (!terms->seek(bytes_ref::nil, *state->cookie)) {
    return score_doc_iterator::empty();
  }

  /* return iterator */
  return score_doc_iterator::make<basic_score_iterator>(
    rdr, 
    *state->reader,
    this->attributes(), 
    terms->postings(ord.features()), 
    ord, 
    state->estimation
  );
}

NS_END // ROOT
