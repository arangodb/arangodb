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

#include "range_filter.hpp"

#include <boost/functional/hash.hpp>

#include "shared.hpp"
#include "multiterm_query.hpp"
#include "term_query.hpp"
#include "index/index_reader.hpp"
#include "analysis/token_attributes.hpp"

NS_LOCAL

template<typename Comparer>
void collect_terms(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    irs::multiterm_query::states_t& states,
    irs::limited_sample_scorer& scorer,
    Comparer cmp) {
  auto& value = terms.value();

  if (cmp(value)) {
    // read attributes
    terms.read();

    // get state for current segment
    auto& state = states.insert(segment);
    state.reader = &field;

    // get term metadata
    auto& meta = terms.attributes().get<irs::term_meta>();
    const decltype(irs::term_meta::docs_count) NO_DOCS = 0;

    // NOTE: we can't use reference to 'docs_count' here, like
    // 'const auto& docs_count = meta ? meta->docs_count : NO_DOCS;'
    // since not gcc4.9 nor msvc2015-2019 can handle this correctly
    // probably due to broken optimization
    const auto* docs_count = meta ? &meta->docs_count : &NO_DOCS;

    do {
      // fill scoring candidates
      scorer.collect(*docs_count, state.count++, state, segment, terms);
      state.estimation += *docs_count;

      if (!terms.next()) {
        break;
      }

      // read attributes
      terms.read();
    } while (cmp(value));
  }
}

NS_END

NS_ROOT

DEFINE_FILTER_TYPE(by_range)
DEFINE_FACTORY_DEFAULT(by_range)

by_range::by_range() noexcept
  : filter(by_range::type()) {
}

bool by_range::equals(const filter& rhs) const noexcept {
  const by_range& trhs = static_cast<const by_range&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && rng_ == trhs.rng_;
}

size_t by_range::hash() const noexcept {
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
    boost_t boost,
    const attribute_view& /*ctx*/) const {
  boost *= this->boost();
  //TODO: optimize unordered case
  // - seek to min
  // - get ordinal position of the term
  // - seek to max
  // - get ordinal position of the term

  if (rng_.min_type != BoundType::UNBOUNDED
      && rng_.max_type != BoundType::UNBOUNDED
      && rng_.min == rng_.max) {

    if (rng_.min_type == rng_.max_type && rng_.min_type == BoundType::INCLUSIVE) {
      // degenerated case
      return term_query::make(index, ord, boost, fld_, rng_.min);
    }

    // can't satisfy conditon
    return prepared::empty();
  }

  limited_sample_scorer scorer(ord.empty() ? 0 : scored_terms_limit()); // object for collecting order stats
  multiterm_query::states_t states(index.size());

  const string_ref field = this->field();

  // iterate over the segments
  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      // can't find field with the specified name
      continue;
    }

    auto terms = reader->iterator();
    bool res = false;

    // seek to min
    switch (rng_.min_type) {
      case BoundType::UNBOUNDED:
        res = terms->next();
        break;
      case BoundType::INCLUSIVE:
        res = seek_min<true>(*terms, rng_.min);
        break;
      case BoundType::EXCLUSIVE:
        res = seek_min<false>(*terms, rng_.min);
        break;
    }

    if (!res) {
      // reached the end, nothing to collect
      continue;
    }

    // now we are on the target or the next term
    const irs::bytes_ref max = rng_.max;

    switch (rng_.max_type) {
      case BoundType::UNBOUNDED:
        ::collect_terms(
          segment, *reader, *terms, states, scorer, [](const bytes_ref&) {
            return true;
        });
        break;
      case BoundType::INCLUSIVE:
        ::collect_terms(
          segment, *reader, *terms, states, scorer, [max](const bytes_ref& term) {
            return term <= max;
        });
        break;
      case BoundType::EXCLUSIVE:
        ::collect_terms(
          segment, *reader, *terms, states, scorer, [max](const bytes_ref& term) {
            return term < max;
        });
        break;
    }
  }

  std::vector<bstring> stats;
  scorer.score(index, ord, stats);

  return memory::make_shared<multiterm_query>(std::move(states), std::move(stats), boost);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
