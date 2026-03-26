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

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "search/filter_visitor.hpp"
#include "search/limited_sample_collector.hpp"
#include "search/term_filter.hpp"
#include "shared.hpp"

namespace irs {
namespace {

template<typename Visitor, typename Comparer>
void collect_terms(const SubReader& segment, const term_reader& field,
                   seek_term_iterator& terms, Visitor& visitor, Comparer cmp) {
  auto* term = irs::get<term_attribute>(terms);

  if (IRS_UNLIKELY(!term)) {
    return;
  }

  if (cmp(term->value)) {
    // read attributes
    terms.read();

    visitor.prepare(segment, field, terms);

    do {
      visitor.visit(kNoBoost);

      if (!terms.next()) {
        break;
      }

      terms.read();
    } while (cmp(term->value));
  }
}

template<typename Visitor>
void VisitImpl(const SubReader& segment, const term_reader& reader,
               const by_range_options::range_type& rng, Visitor& visitor) {
  auto terms = reader.iterator(SeekMode::NORMAL);

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
  const bytes_view max = rng.max;

  switch (rng.max_type) {
    case BoundType::UNBOUNDED:
      collect_terms(segment, reader, *terms, visitor,
                    [](bytes_view) { return true; });
      break;
    case BoundType::INCLUSIVE:
      collect_terms(segment, reader, *terms, visitor,
                    [max](bytes_view term) { return term <= max; });
      break;
    case BoundType::EXCLUSIVE:
      collect_terms(segment, reader, *terms, visitor,
                    [max](bytes_view term) { return term < max; });
      break;
  }
}

}  // namespace

filter::prepared::ptr by_range::prepare(const PrepareContext& ctx,
                                        std::string_view field,
                                        const options_type::range_type& rng,
                                        size_t scored_terms_limit) {
  // TODO: optimize unordered case
  //  - seek to min
  //  - get ordinal position of the term
  //  - seek to max
  //  - get ordinal position of the term

  if (rng.min_type != BoundType::UNBOUNDED &&
      rng.max_type != BoundType::UNBOUNDED && rng.min == rng.max) {
    if (rng.min_type == rng.max_type && rng.min_type == BoundType::INCLUSIVE) {
      // degenerated case
      return by_term::prepare(ctx, field, rng.min);
    }

    // can't satisfy conditon
    return prepared::empty();
  }

  // object for collecting order stats
  limited_sample_collector<term_frequency> collector(
    ctx.scorers.empty() ? 0 : scored_terms_limit);
  MultiTermQuery::States states{ctx.memory, ctx.index.size()};
  multiterm_visitor mtv{collector, states};

  for (const auto& segment : ctx.index) {
    if (const auto* reader = segment.field(field); reader) {
      VisitImpl(segment, *reader, rng, mtv);
    }
  }

  MultiTermQuery::Stats stats{{ctx.memory}};
  collector.score(ctx.index, ctx.scorers, stats);

  return memory::make_tracked<MultiTermQuery>(ctx.memory, std::move(states),
                                              std::move(stats), ctx.boost,
                                              ScoreMergeType::kSum, size_t{1});
}

void by_range::visit(const SubReader& segment, const term_reader& reader,
                     const options_type::range_type& rng,
                     filter_visitor& visitor) {
  VisitImpl(segment, reader, rng, visitor);
}

}  // namespace irs
