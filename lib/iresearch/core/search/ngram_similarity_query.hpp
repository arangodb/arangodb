////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Abramov
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "search/filter.hpp"
#include "search/prepared_state_visitor.hpp"
#include "search/states/ngram_state.hpp"
#include "search/states_cache.hpp"

namespace irs {

using NGramStates = StatesCache<NGramState>;

// Prepared ngram similarity query implementation
class NGramSimilarityQuery : public filter::prepared {
 public:
  // returns set of features required for filter
  static constexpr IndexFeatures kRequiredFeatures =
    IndexFeatures::FREQ | IndexFeatures::POS;

  NGramSimilarityQuery(size_t min_match_count, NGramStates&& states,
                       bstring&& stats, score_t boost = kNoBoost)
    : min_match_count_{min_match_count},
      states_{std::move(states)},
      stats_{std::move(stats)},
      boost_{boost} {}

  doc_iterator::ptr execute(const ExecutionContext& ctx) const final;

  void visit(const SubReader& segment, PreparedStateVisitor& visitor,
             score_t boost) const final {
    if (const auto* state = states_.find(segment); state) {
      visitor.Visit(*this, *state, boost * boost_);
    }
  }

  score_t boost() const noexcept final { return boost_; }

  doc_iterator::ptr ExecuteWithOffsets(const SubReader& rdr) const;

 private:
  size_t min_match_count_;
  NGramStates states_;
  bstring stats_;
  score_t boost_;
};

}  // namespace irs
