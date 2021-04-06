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

#include "prefix_filter.hpp"

#include "shared.hpp"
#include "search/limited_sample_collector.hpp"
#include "search/states_cache.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/iterators.hpp"

namespace {

using namespace irs;

template<typename Visitor>
void visit(
    const sub_reader& segment,
    const term_reader& reader,
    const bytes_ref& prefix,
    Visitor& visitor) {
  // find term
  auto terms = reader.iterator();

  // seek to prefix
  if (IRS_UNLIKELY(!terms) || SeekResult::END == terms->seek_ge(prefix)) {
    return;
  }

  const auto& value = terms->value();
  if (starts_with(value, prefix)) {
    terms->read();

    visitor.prepare(segment, reader, *terms);

    do {
      visitor.visit(no_boost());

      if (!terms->next()) {
        break;
      }

      terms->read();
    } while (starts_with(value, prefix));
  }
}

}

namespace iresearch {

DEFINE_FACTORY_DEFAULT(by_prefix)

/*static*/ filter::prepared::ptr by_prefix::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& prefix,
    size_t scored_terms_limit) {
  limited_sample_collector<term_frequency> collector(ord.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  multiterm_query::states_t states(index);
  multiterm_visitor<multiterm_query::states_t> mtv(collector, states);

  // iterate over the segments
  for (const auto& segment: index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    ::visit(segment, *reader, prefix, mtv);
  }

  std::vector<bstring> stats;
  collector.score(index, ord, stats);

  return memory::make_managed<multiterm_query>(
    std::move(states), std::move(stats),
    boost, sort::MergeType::AGGREGATE);
}

/*static*/ void by_prefix::visit(
    const sub_reader& segment,
    const term_reader& reader,
    const bytes_ref& prefix,
    filter_visitor& visitor) {
  ::visit(segment, reader, prefix, visitor);
}

} // ROOT
