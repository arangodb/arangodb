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
#include "filter_visitor.hpp"
#include "multiterm_query.hpp"
#include "term_query.hpp"
#include "index/index_reader.hpp"
#include "analysis/token_attributes.hpp"

NS_LOCAL

using namespace irs;

template<typename Visitor, typename Comparer>
void collect_terms(
    seek_term_iterator& terms,
    Visitor& visitor,
    Comparer cmp) {
  auto& value = terms.value();

  if (cmp(value)) {
    // read attributes
    terms.read();

    visitor.prepare(terms);

    do {
      visitor.visit();

      if (!terms.next()) {
        break;
      }

      terms.read();
    } while (cmp(value));
  }
}

template<typename Visitor>
void visit(
    const term_reader& reader,
    const by_range::range_t& rng,
    Visitor& visitor) {
  auto terms = reader.iterator();

  if (IRS_UNLIKELY(!terms)) {
    return;
  }

  auto res = false;

  // seek to min
  switch (rng.min_type) {
    case BoundType::UNBOUNDED:
      res = terms->next();
      break;
    case BoundType::INCLUSIVE:
      res = seek_min<true>(*terms, rng.min);
      break;
    case BoundType::EXCLUSIVE:
      res = seek_min<false>(*terms, rng.min);
      break;
  }

  if (!res) {
    // reached the end, nothing to collect
    return;
  }

  // now we are on the target or the next term
  const bytes_ref max = rng.max;

  switch (rng.max_type) {
    case BoundType::UNBOUNDED:
      ::collect_terms(
        *terms, visitor, [](const bytes_ref&) {
          return true;
      });
      break;
    case BoundType::INCLUSIVE:
      ::collect_terms(
        *terms, visitor, [max](const bytes_ref& term) {
          return term <= max;
      });
      break;
    case BoundType::EXCLUSIVE:
      ::collect_terms(
        *terms, visitor, [max](const bytes_ref& term) {
          return term < max;
      });
      break;
  }
}

NS_END

NS_ROOT

DEFINE_FILTER_TYPE(by_range)
DEFINE_FACTORY_DEFAULT(by_range)

/*static*/ filter::prepared::ptr by_range::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const range_t& rng,
    size_t scored_terms_limit) {
  //TODO: optimize unordered case
  // - seek to min
  // - get ordinal position of the term
  // - seek to max
  // - get ordinal position of the term

  if (rng.min_type != BoundType::UNBOUNDED
      && rng.max_type != BoundType::UNBOUNDED
      && rng.min == rng.max) {

    if (rng.min_type == rng.max_type && rng.min_type == BoundType::INCLUSIVE) {
      // degenerated case
      return term_query::make(index, ord, boost, field, rng.min);
    }

    // can't satisfy conditon
    return prepared::empty();
  }

  limited_sample_collector<term_frequency> collector(ord.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  multiterm_query::states_t states(index.size());

  // iterate over the segments
  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      // can't find field with the specified name
      continue;
    }

    multiterm_visitor<multiterm_query::states_t> mtv(segment, *reader, collector, states);

    ::visit(*reader, rng, mtv);
  }

  std::vector<bstring> stats;
  collector.score(index, ord, stats);

  return memory::make_shared<multiterm_query>(
    std::move(states), std::move(stats),
    boost, sort::MergeType::AGGREGATE);
}

/*static*/ void by_range::visit(
    const term_reader& reader,
    const range_t& rng,
    filter_visitor& visitor) {
  ::visit(reader, rng, visitor);
}

by_range::by_range() noexcept
  : filter(by_range::type()) {
}

bool by_range::equals(const filter& rhs) const noexcept {
  const by_range& trhs = static_cast<const by_range&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && rng_ == trhs.rng_;
}

size_t hash_value(const by_range::range_t& rng) {
  size_t seed = 0;
  ::boost::hash_combine(seed, rng.min);
  ::boost::hash_combine(seed, rng.min_type);
  ::boost::hash_combine(seed, rng.max);
  ::boost::hash_combine(seed, rng.max_type);
  return seed;
}

size_t by_range::hash() const noexcept {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, fld_);
  ::boost::hash_combine(seed, rng_);
  return seed;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
