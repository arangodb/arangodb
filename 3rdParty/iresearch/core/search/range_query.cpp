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
#include "range_query.hpp"
#include "disjunction.hpp"
#include "score_doc_iterators.hpp"
#include "index/index_reader.hpp"

namespace {
  template<
    typename IteratorWrapper,
    typename IteratorTraits = iresearch::iterator_traits<IteratorWrapper>
  >
  class masking_disjunction: public iresearch::detail::disjunction<IteratorWrapper, IteratorTraits> {
    typedef iresearch::detail::disjunction<IteratorWrapper, IteratorTraits> parent;
   public:
    typedef std::unordered_set<typename parent::doc_iterator::element_type*> doc_itr_score_mask_t;
    masking_disjunction(
      typename parent::doc_iterators_t&& doc_itrs,
      doc_itr_score_mask_t&& doc_itr_score_mask, //score only these itrs
      const iresearch::order::prepared& order,
      iresearch::cost::cost_t estimation
    ): parent(std::move(doc_itrs), order, estimation), doc_itr_score_mask_(doc_itr_score_mask) {
    }
    virtual void score_add_impl(iresearch::byte_type* dst, typename parent::doc_iterator& src) {
      if (doc_itr_score_mask_.find(src.get()) != doc_itr_score_mask_.end()) {
        parent::score_add_impl(dst, src);
      }
    }
   private:
    doc_itr_score_mask_t doc_itr_score_mask_;
  };
}

NS_ROOT

limited_sample_scorer::limited_sample_scorer(size_t scored_terms_limit):
  scored_terms_limit_(scored_terms_limit) {
}

void limited_sample_scorer::collect(
  size_t priority, // priority of this entry, lowest priority removed first
  size_t scored_state_id, // state identifier used for querying of attributes
  iresearch::range_state& scored_state, // state containing this scored term
  const iresearch::sub_reader& reader, // segment reader for the current term
  seek_term_iterator::cookie_ptr&& cookie // term-reader term offset cache
) {
  scored_states_.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(priority),
    std::forward_as_tuple(reader, scored_state, scored_state_id, std::move(cookie))
  );

  // if too many candidates then remove least significant
  if (scored_states_.size() > scored_terms_limit_) {
    scored_states_.erase(scored_states_.begin());
  }
}

void limited_sample_scorer::score(order::prepared::stats& stats) {
  // iterate over the scoring candidates
  for (auto& entry: scored_states_) {
    auto& scored_term_state = entry.second;
    auto terms = scored_term_state.state.reader->iterator();

    // find term using cached state
    // use bytes_ref::nil here since we just "jump" to cached state,
    // and we are not interested in term value itself
    if (!terms->seek(bytes_ref::nil, *scored_term_state.cookie)) {
      continue; // some internal error that caused the term to disapear
    }

    auto& term = terms->attributes();
    auto& segment = scored_term_state.sub_reader;
    auto& field = *scored_term_state.state.reader;

    // collect field level statistic 
    stats.field(segment, field);

    // collect term level statistics
    stats.term(term);

    attribute_store scored_term_attributes;

    // apply/store order stats
    stats.finish(scored_term_state.sub_reader, scored_term_attributes);

    scored_term_state.state.scored_states.emplace(
      scored_term_state.state_offset, std::move(scored_term_attributes)
    );
  }
}

range_query::range_query(states_t&& states) 
  : states_(std::move(states)) {
}

score_doc_iterator::ptr range_query::execute(
    const sub_reader& rdr,
    const order::prepared& ord) const {
  typedef masking_disjunction<score_wrapper<score_doc_iterator::ptr>> disjunction_t;

  /* get term state for the specified reader */
  auto state = states_.find(rdr);
  if (!state) {
    /* invalid state */
    return score_doc_iterator::empty();
  }

  /* get terms iterator */
  auto terms = state->reader->iterator();

  /* find min term using cached state */
  if (!terms->seek(state->min_term, *(state->min_cookie))) {
    return score_doc_iterator::empty();
  }

  /* prepared disjunction */
  disjunction_t::doc_iterators_t itrs;
  itrs.reserve(state->count);

  /* get required features for order */
  auto& features = ord.features();

  // set of doc_iterators that should be scored
  disjunction_t::doc_itr_score_mask_t doc_itr_score_mask;

  /* iterator for next "state.count" terms */
  for (size_t i = 0, end = state->count; i < end; ++i) {
    auto scored_state_itr = state->scored_states.find(i);

    if (scored_state_itr == state->scored_states.end()) {
      itrs.emplace_back(score_doc_iterator::make<basic_score_iterator>(
        rdr,
        *state->reader,
        attribute_store::empty_instance(),
        std::move(terms->postings(features)),
        ord,
        state->estimation
      ));
    }
    else {
      itrs.emplace_back(score_doc_iterator::make<basic_score_iterator>(
        rdr,
        *state->reader,
        scored_state_itr->second,
        std::move(terms->postings(features)),
        ord,
        state->estimation
      ));
      doc_itr_score_mask.emplace(itrs.back().get());
    }

    terms->next();
  }

  if (itrs.empty()) {
    return score_doc_iterator::empty();
  }

  return score_doc_iterator::make<disjunction_t>(
    std::move(itrs), std::move(doc_itr_score_mask), ord, state->estimation
  );
}

NS_END // ROOT
