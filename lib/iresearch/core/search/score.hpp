////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "search/scorer.hpp"
#include "utils/attributes.hpp"

namespace irs {

// Represents a score related for the particular document
// min score set by document consumers
// max score set by document producers
struct score : attribute, ScoreFunction {
  static const score kNoScore;

  static constexpr std::string_view type_name() noexcept {
    return "irs::score";
  }

  template<typename Provider>
  static const score& get(const Provider& attrs) {
    const auto* score = irs::get<irs::score>(attrs);
    return score ? *score : kNoScore;
  }

  using ScoreFunction::operator=;

  // For disjunction/conjunction it's just sum of sub-iterators max score
  // For iterator without score it depends on count of documents in iterator
  // For wanderator it's max score for whole skip-list
  // TODO(MBkkt) tail better here and not affect correctness
  //  but to support it we need to know max value in the tail blocks.
  //  Open question: how do it without read next blocks?
  // TODO(MBkkt) At least when iterator exhausted, we could set it to zero.
  struct UpperBounds {
    score_t tail = std::numeric_limits<score_t>::max();
    score_t leaf = std::numeric_limits<score_t>::max();
#ifdef IRESEARCH_TEST
    std::span<const score_t> levels;  // levels.back() == leaf
#endif
  } max;
};

using ScoreFunctions = SmallVector<ScoreFunction, 2>;

// Prepare scorer for each of the bucket.
ScoreFunctions PrepareScorers(std::span<const ScorerBucket> buckets,
                              const ColumnProvider& segment,
                              const term_reader& field, const byte_type* stats,
                              const attribute_provider& doc, score_t boost);

// Compiles a set of prepared scorers into a single score function.
ScoreFunction CompileScorers(ScoreFunctions&& scorers);

void CompileScore(irs::score& score, std::span<const ScorerBucket> buckets,
                  const ColumnProvider& segment, const term_reader& field,
                  const byte_type* stats, const attribute_provider& doc,
                  score_t boost);

// Prepare empty collectors, i.e. call collect(...) on each of the
// buckets without explicitly collecting field or term statistics,
// e.g. for 'all' filter.
void PrepareCollectors(std::span<const ScorerBucket> order, byte_type* stats);

}  // namespace irs
