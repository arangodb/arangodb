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
#include "range_filter.hpp"
#include "range_query.hpp"
#include "index/index_reader.hpp"
#include "analysis/token_attributes.hpp"

#include <boost/functional/hash.hpp>

NS_ROOT
NS_BEGIN(detail)

template<typename Comparer>
void collect_terms(
    const sub_reader& reader, const term_reader& tr, 
    seek_term_iterator& terms, range_query::states_t& states, 
    limited_sample_scorer* scorer,
    Comparer cmp) {
  if (cmp(terms)) {
    /* read attributes */
    terms.read();

    /* get state for current segment */
    auto& state = states.insert(reader);
    state.reader = &tr;
    state.min_term = terms.value();
    state.min_cookie = terms.cookie();

    // get term metadata
    auto& meta = terms.attributes().get<term_meta>();

    do {
      // fill scoring candidates
      if (scorer) {
        scorer->collect(meta ? meta->docs_count : 0, state.count, state, reader, terms.cookie());
      }

      ++state.count;

      if (meta) {
        state.estimation += meta->docs_count;
      }

      if (!terms.next()) {
        break;
      }

      /* read attributes */
      terms.read();
    } while (cmp(terms));
  }
}

NS_END // detail

DEFINE_FILTER_TYPE(by_range)
DEFINE_FACTORY_DEFAULT(by_range)

by_range::by_range():
  filter(by_range::type()) {
}

bool by_range::equals(const filter& rhs) const {
  const by_range& trhs = static_cast<const by_range&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && rng_ == trhs.rng_;
}

size_t by_range::hash() const {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, fld_);
  ::boost::hash_combine(seed, rng_.min);
  ::boost::hash_combine(seed, rng_.min_type);
  ::boost::hash_combine(seed, rng_.max);
  ::boost::hash_combine(seed, rng_.max_type);
  return seed;
}

filter::prepared::ptr by_range::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const {
  limited_sample_scorer scorer_instance(scored_terms_limit_); // object for collecting order stats
  limited_sample_scorer* scorer = ord.empty() ? nullptr : &scorer_instance;
  range_query::states_t states(rdr.size());

  /* iterate over the segments */
  const string_ref field = fld_;
  for (const auto& sr : rdr) {
    /* get term dictionary for field */
    const term_reader* tr = sr.field(field);
    if (!tr) {
      continue;
    }

    /* seek to min */
    seek_term_iterator::ptr terms = tr->iterator();

    //TODO: optimize empty unordered case. seek to min, 
    //get ordinal position of the term, seek max, 
    //get ordinal position of the term.

    auto res = terms->seek_ge(rng_.min);
    if (SeekResult::END == res) {
      /* have reached the end of the segment */
      continue;
    }

    if (SeekResult::FOUND == res) {
      if (Bound_Type::EXCLUSIVE == rng_.min_type && !terms->next()) {
        continue;
      }
    }

    /* now we are on the target term 
     * or on the next term after the target term */
    auto& max = rng_.max;

    if (Bound_Type::UNBOUNDED == rng_.max_type) {
      detail::collect_terms(
        sr, *tr, *terms, states, scorer, [](const term_iterator&)->bool {
          return true; 
      });
    } else if (Bound_Type::INCLUSIVE == rng_.max_type) {
      detail::collect_terms(
        sr, *tr, *terms, states, scorer, [&max](const term_iterator& terms)->bool {
          return terms.value() <= max;
      });
    } else { // Bound_Type::EXCLUSIVE == rng_.max_type
      detail::collect_terms(
        sr, *tr, *terms, states, scorer, [&max](const term_iterator& terms)->bool {
          return terms.value() < max;
      });
    }
  }

  if (scorer) {
    auto stats = ord.prepare_stats();

    scorer->score(stats);
  }

  auto q = memory::make_unique<range_query>(std::move(states));

  /* apply boost */
  iresearch::boost::apply(q->attributes(), this->boost() * boost);

  return std::move(q);
}

NS_END // ROOT
