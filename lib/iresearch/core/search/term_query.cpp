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

#include "term_query.hpp"

#include "index/field_meta.hpp"
#include "index/index_reader.hpp"
#include "search/prepared_state_visitor.hpp"
#include "search/score.hpp"

namespace irs {

TermQuery::TermQuery(States&& states, bstring&& stats, score_t boost)
  : states_{std::move(states)}, stats_{std::move(stats)}, boost_{boost} {}

doc_iterator::ptr TermQuery::execute(const ExecutionContext& ctx) const {
  const auto& rdr = ctx.segment;
  const auto& ord = ctx.scorers;
  // Get term state for the specified reader
  const auto* state = states_.find(rdr);

  if (IRS_UNLIKELY(!state)) {  // Invalid state
    return doc_iterator::empty();
  }

  const auto* reader = state->reader;
  IRS_ASSERT(reader);
  doc_iterator::ptr docs;
  auto ord_buckets = ord.buckets();
  if (ctx.wand.Enabled() && !ord_buckets.empty()) {
    const auto& front = ord_buckets.front();
    if (front.bucket != nullptr) {
      docs = reader->wanderator(
        *state->cookie, ord.features(), {[&](const attribute_provider& attrs) {
          return front.bucket->prepare_scorer(
            rdr, state->reader->meta().features,
            stats_.c_str() + front.stats_offset, attrs, boost_);
        }},
        ctx.wand);
    }
  }
  if (!docs) {
    docs = reader->postings(*state->cookie, ord.features());
  }
  IRS_ASSERT(docs);

  if (!ord_buckets.empty()) {
    auto* score = irs::get_mutable<irs::score>(docs.get());
    IRS_ASSERT(score);
    CompileScore(*score, ord_buckets, rdr, *state->reader, stats_.c_str(),
                 *docs, boost_);
  }

  return docs;
}

void TermQuery::visit(const SubReader& segment, PreparedStateVisitor& visitor,
                      score_t boost) const {
  if (const auto* state = states_.find(segment); state) {
    visitor.Visit(*this, *state, boost * boost_);
  }
}

}  // namespace irs
