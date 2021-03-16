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

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "search/filter_visitor.hpp"
#include "search/limited_sample_collector.hpp"
#include "search/term_filter.hpp"

namespace {

using namespace irs;

template<typename Visitor, typename Comparer>
void collect_terms(
    const sub_reader& segment,
    const term_reader& field,
    seek_term_iterator& terms,
    Visitor& visitor,
    Comparer cmp) {
  auto& value = terms.value();

  if (cmp(value)) {
    // read attributes
    terms.read();

    visitor.prepare(segment, field, terms);

    do {
      visitor.visit(no_boost());

      if (!terms.next()) {
        break;
      }

      terms.read();
    } while (cmp(value));
  }
}

template<typename Visitor>
void visit(
    const sub_reader& segment,
    const term_reader& reader,
    const by_range_options::range_type& rng,
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
        segment, reader, *terms, visitor, [](const bytes_ref&) {
          return true;
      });
      break;
    case BoundType::INCLUSIVE:
      ::collect_terms(
        segment, reader, *terms, visitor, [max](const bytes_ref& term) {
          return term <= max;
      });
      break;
    case BoundType::EXCLUSIVE:
      ::collect_terms(
        segment, reader, *terms, visitor, [max](const bytes_ref& term) {
          return term < max;
      });
      break;
  }
}

}

namespace iresearch {

DEFINE_FACTORY_DEFAULT(by_range)

/*static*/ filter::prepared::ptr by_range::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const options_type::range_type& rng,
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
      return by_term::prepare(index, ord, boost, field, rng.min);
    }

    // can't satisfy conditon
    return prepared::empty();
  }

  limited_sample_collector<term_frequency> collector(ord.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  multiterm_query::states_t states(index);
  multiterm_visitor<multiterm_query::states_t> mtv(collector, states);

  // iterate over the segments
  for (const auto& segment : index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      // can't find field with the specified name
      continue;
    }

    ::visit(segment, *reader, rng, mtv);
  }

  std::vector<bstring> stats;
  collector.score(index, ord, stats);

  return memory::make_managed<multiterm_query>(
    std::move(states), std::move(stats),
    boost, sort::MergeType::AGGREGATE);
}

/*static*/ void by_range::visit(
    const sub_reader& segment,
    const term_reader& reader,
    const options_type::range_type& rng,
    filter_visitor& visitor) {
  ::visit(segment, reader, rng, visitor);
}

} // ROOT
