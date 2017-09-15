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
#include "multiterm_filter.hpp"
#include "range_query.hpp"
#include "term_query.hpp"
#include "index/index_reader.hpp"
#include "analysis/token_attributes.hpp"

#include <boost/functional/hash.hpp>

NS_LOCAL

template<typename Comparer>
void collect_terms(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    irs::range_query::states_t& states,
    irs::limited_sample_scorer& scorer,
    Comparer cmp) {
  if (cmp(terms)) {
    // read attributes
    terms.read();

    // get state for current segment
    auto& state = states.insert(segment);
    state.reader = &field;
    state.min_term = terms.value();
    state.min_cookie = terms.cookie();
    state.unscored_docs.reset(
      (irs::type_limits<irs::type_t::doc_id_t>::min)() + segment.docs_count()
    ); // highest valid doc_id in segment

    // get term metadata
    auto& meta = terms.attributes().get<irs::term_meta>();

    do {
      // fill scoring candidates
      scorer.collect(meta ? meta->docs_count : 0, state.count, state, segment, terms);
      ++state.count;

      if (meta) {
        state.estimation += meta->docs_count;
      }

      if (!terms.next()) {
        break;
      }

      // read attributes
      terms.read();
    } while (cmp(terms));
  }
}

NS_END // LOCAL

NS_ROOT

DEFINE_FILTER_TYPE(by_range)
DEFINE_FACTORY_DEFAULT(by_range)

by_range::by_range() NOEXCEPT
  : filter(by_range::type()) {
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
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost) const {
  //TODO: optimize unordered case
  // - seek to min
  // - get ordinal position of the term
  // - seek to max
  // - get ordinal position of the term

  if (rng_.min_type != Bound_Type::UNBOUNDED
      && rng_.max_type != Bound_Type::UNBOUNDED
      && rng_.min == rng_.max) {

    if (rng_.min_type == rng_.max_type && rng_.min_type == Bound_Type::INCLUSIVE) {
      // degenerated case
      return term_query::make(index, ord, boost*this->boost(), fld_, rng_.min);
    }

    // can't satisfy conditon
    return prepared::empty();
  }

  limited_sample_scorer scorer(ord.empty() ? 0 : scored_terms_limit_); // object for collecting order stats
  range_query::states_t states(index.size());

  // iterate over the segments
  const string_ref field_name = fld_;
  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* field = segment.field(field_name);

    if (!field) {
      // can't find field with the specified name
      continue;
    }

    auto terms = field->iterator();
    bool res = false;

    // seek to min
    switch (rng_.min_type) {
      case Bound_Type::UNBOUNDED:
        res = terms->next();
        break;
      case Bound_Type::INCLUSIVE:
        res = seek_min<true>(*terms, rng_.min);
        break;
      case Bound_Type::EXCLUSIVE:
        res = seek_min<false>(*terms, rng_.min);
        break;
      default:
        assert(false);
    }

    if (!res) {
      // reached the end, nothing to collect
      continue;
    }

    // now we are on the target or the next term
    const irs::bytes_ref max = rng_.max;

    switch (rng_.max_type) {
      case Bound_Type::UNBOUNDED:
        ::collect_terms(
          segment, *field, *terms, states, scorer, [](const term_iterator&) {
            return true;
        });
        break;
      case Bound_Type::INCLUSIVE:
        ::collect_terms(
          segment, *field, *terms, states, scorer, [max](const term_iterator& terms) {
            return terms.value() <= max;
        });
        break;
      case Bound_Type::EXCLUSIVE:
        ::collect_terms(
          segment, *field, *terms, states, scorer, [max](const term_iterator& terms) {
            return terms.value() < max;
        });
        break;
      default:
        assert(false);
    }
  }

  scorer.score(index, ord);

  auto q = memory::make_unique<range_query>(std::move(states));

  // apply boost
  irs::boost::apply(q->attributes(), this->boost() * boost);

  return std::move(q);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
