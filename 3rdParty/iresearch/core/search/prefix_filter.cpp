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
#include "prefix_filter.hpp"
#include "range_query.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"

#include <boost/functional/hash.hpp>

NS_ROOT

filter::prepared::ptr by_prefix::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const {
  limited_sample_scorer scorer(scored_terms_limit_); // object for collecting order stats
  range_query::states_t states(rdr.size());  

  auto& prefix = term();

  /* iterate over the segments */
  const string_ref field = this->field();
  for (const auto& sr : rdr ) {
    /* get term dictionary for field */
    const term_reader* tr = sr.field(field);
    if (!tr) {
      continue;
    }

    seek_term_iterator::ptr terms = tr->iterator();

    /* seek to prefix */
    if (SeekResult::END == terms->seek_ge(prefix)) {
      continue;
    }

    /* get term metadata */
    auto& meta = terms->attributes().get<term_meta>();

    if (starts_with(terms->value(), prefix)) {
      terms->read();

      /* get state for current segment */
      auto& state = states.insert(sr);
      state.reader = tr;
      state.min_term = terms->value();
      state.min_cookie = terms->cookie();

      do {
        // fill scoring candidates
        if (!ord.empty()) {
          scorer.collect(meta ? meta->docs_count : 0, state.count, state, sr, terms->cookie());
        }

        ++state.count;

        /* collect cost */
        if (meta) {
          state.estimation += meta->docs_count;
        }

        if (!terms->next()) {
          break;
        }

        terms->read();
      } while (starts_with(terms->value(), prefix));
    }
  }

  if (!ord.empty()) {
    auto stats = ord.prepare_stats();

    scorer.score(stats);
  }

  auto q = memory::make_unique<range_query>(std::move(states));

  /* apply boost */
  iresearch::boost::apply(q->attributes(), this->boost() * boost);

  return std::move(q);
}

DEFINE_FILTER_TYPE(by_prefix)
DEFINE_FACTORY_DEFAULT(by_prefix);

by_prefix::by_prefix() : by_term(by_prefix::type()) {
}

size_t by_prefix::hash() const {
  size_t seed = 0;
  ::boost::hash_combine(seed, by_term::hash());
  ::boost::hash_combine(seed, scored_terms_limit_);
  return seed;
}

bool by_prefix::equals(const filter& rhs) const {
  const auto& trhs = static_cast<const by_prefix&>(rhs);
  return by_term::equals(rhs) && scored_terms_limit_ == trhs.scored_terms_limit_;
}

NS_END // ROOT
