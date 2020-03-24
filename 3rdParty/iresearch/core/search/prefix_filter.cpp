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

#include <boost/functional/hash.hpp>

#include "shared.hpp"
#include "filter_visitor.hpp"
#include "multiterm_query.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/iterators.hpp"

NS_ROOT

NS_LOCAL

template<typename Visitor>
void visit(
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

    visitor.prepare(terms);

    do {
      visitor.visit();

      if (!terms->next()) {
        break;
      }

      terms->read();
    } while (starts_with(value, prefix));
  }
}

NS_END

/*static*/ void by_prefix::visit(
    const term_reader& reader,
    const bytes_ref& prefix,
    filter_visitor& visitor) {
  irs::visit(reader, prefix, visitor);
}

DEFINE_FILTER_TYPE(by_prefix)
DEFINE_FACTORY_DEFAULT(by_prefix)

/*static*/ filter::prepared::ptr by_prefix::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& prefix,
    size_t scored_terms_limit) {
  limited_sample_scorer scorer(ord.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  multiterm_query::states_t states(index.size());

  // iterate over the segments
  for (const auto& segment: index) {
    // get term dictionary for field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    multiterm_visitor mtv(segment, *reader, scorer, states);

    irs::visit(*reader, prefix, mtv);
  }

  std::vector<bstring> stats;
  scorer.score(index, ord, stats);

  return memory::make_shared<multiterm_query>(std::move(states), std::move(stats), boost);
}

by_prefix::by_prefix() noexcept : by_prefix(by_prefix::type()) { }

size_t by_prefix::hash() const noexcept {
  size_t seed = 0;
  ::boost::hash_combine(seed, by_term::hash());
  ::boost::hash_combine(seed, scored_terms_limit_);
  return seed;
}

bool by_prefix::equals(const filter& rhs) const noexcept {
  const auto& impl = static_cast<const by_prefix&>(rhs);
  return by_term::equals(rhs) && scored_terms_limit_ == impl.scored_terms_limit_;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
