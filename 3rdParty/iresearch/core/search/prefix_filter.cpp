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
#include "prefix_filter.hpp"
#include "range_query.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/iterators.hpp"

#include <boost/functional/hash.hpp>

NS_ROOT

filter::prepared::ptr by_prefix::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& /*ctx*/) const {
  limited_sample_scorer scorer(ord.empty() ? 0 : scored_terms_limit_); // object for collecting order stats
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
      state.unscored_docs.reset((type_limits<type_t::doc_id_t>::min)() + sr.docs_count()); // highest valid doc_id in reader

      do {
        // fill scoring candidates
        scorer.collect(meta ? meta->docs_count : 0, state.count, state, sr, *terms);
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

  scorer.score(rdr, ord);

  return memory::make_shared<range_query>(std::move(states), this->boost() * boost);
}

DEFINE_FILTER_TYPE(by_prefix)
DEFINE_FACTORY_DEFAULT(by_prefix)

by_prefix::by_prefix() NOEXCEPT
  : by_term(by_prefix::type()) {
}

size_t by_prefix::hash() const NOEXCEPT {
  size_t seed = 0;
  ::boost::hash_combine(seed, by_term::hash());
  ::boost::hash_combine(seed, scored_terms_limit_);
  return seed;
}

bool by_prefix::equals(const filter& rhs) const NOEXCEPT {
  const auto& trhs = static_cast<const by_prefix&>(rhs);
  return by_term::equals(rhs) && scored_terms_limit_ == trhs.scored_terms_limit_;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
