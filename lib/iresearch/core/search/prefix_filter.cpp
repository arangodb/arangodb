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

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/iterators.hpp"
#include "search/limited_sample_collector.hpp"
#include "search/states_cache.hpp"
#include "shared.hpp"

namespace irs {
namespace {

template<typename Visitor>
void VisitImpl(const SubReader& segment, const term_reader& reader,
               bytes_view prefix, Visitor& visitor) {
  auto terms = reader.iterator(SeekMode::NORMAL);

  // seek to prefix
  if (IRS_UNLIKELY(!terms) || SeekResult::END == terms->seek_ge(prefix)) {
    return;
  }

  auto* term = irs::get<term_attribute>(*terms);

  if (IRS_UNLIKELY(!term)) {
    return;
  }

  if (term->value.starts_with(prefix)) {
    terms->read();

    visitor.prepare(segment, reader, *terms);

    do {
      visitor.visit(kNoBoost);

      if (!terms->next()) {
        break;
      }

      terms->read();
    } while (term->value.starts_with(prefix));
  }
}

}  // namespace

filter::prepared::ptr by_prefix::prepare(const PrepareContext& ctx,
                                         std::string_view field,
                                         bytes_view prefix,
                                         size_t scored_terms_limit) {
  // object for collecting order stats
  limited_sample_collector<term_frequency> collector(
    ctx.scorers.empty() ? 0 : scored_terms_limit);
  MultiTermQuery::States states{ctx.memory, ctx.index.size()};
  multiterm_visitor mtv{collector, states};

  for (const auto& segment : ctx.index) {
    if (const auto* reader = segment.field(field); reader) {
      VisitImpl(segment, *reader, prefix, mtv);
    }
  }

  MultiTermQuery::Stats stats{{ctx.memory}};
  collector.score(ctx.index, ctx.scorers, stats);

  return memory::make_tracked<MultiTermQuery>(ctx.memory, std::move(states),
                                              std::move(stats), ctx.boost,
                                              ScoreMergeType::kSum, size_t{1});
}

void by_prefix::visit(const SubReader& segment, const term_reader& reader,
                      bytes_view prefix, filter_visitor& visitor) {
  VisitImpl(segment, reader, prefix, visitor);
}

}  // namespace irs
