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

#include "score.hpp"

#include "formats/formats.hpp"
#include "index/field_meta.hpp"
#include "shared.hpp"

namespace irs {

const score score::kNoScore;

ScoreFunctions PrepareScorers(std::span<const ScorerBucket> buckets,
                              const ColumnProvider& segment,
                              const term_reader& field,
                              const byte_type* stats_buf,
                              const attribute_provider& doc, score_t boost) {
  ScoreFunctions scorers;
  scorers.reserve(buckets.size());

  for (const auto& entry : buckets) {
    const auto& bucket = *entry.bucket;

    if (IRS_UNLIKELY(!entry.bucket)) {
      continue;
    }

    auto scorer =
      bucket.prepare_scorer(segment, field.meta().features,
                            stats_buf + entry.stats_offset, doc, boost);

    scorers.emplace_back(std::move(scorer));
  }

  return scorers;
}

// TODO(MBkkt) Do we really need to optimize >= 2 scorers case?

static ScoreFunction CompileScorers(ScoreFunction&& wand,
                                    ScoreFunctions&& tail) {
  IRS_ASSERT(!tail.empty());
  switch (tail.size()) {
    case 0:
      return std::move(wand);
    case 1: {
      struct Ctx final : score_ctx {
        Ctx(ScoreFunction&& wand, ScoreFunction&& tail) noexcept
          : wand{std::move(wand)}, tail{std::move(tail)} {}

        ScoreFunction wand;
        ScoreFunction tail;
      };

      return ScoreFunction::Make<Ctx>(
        [](score_ctx* ctx, score_t* res) noexcept {
          auto* scorers_ctx = static_cast<Ctx*>(ctx);
          IRS_ASSERT(res != nullptr);
          scorers_ctx->wand.Score(res);
          scorers_ctx->tail.Score(res + 1);
        },
        [](score_ctx* ctx, score_t arg) noexcept {
          auto* scorers_ctx = static_cast<Ctx*>(ctx);
          scorers_ctx->wand.Min(arg);
        },
        std::move(wand), std::move(tail.front()));
    }
    default: {
      struct Ctx final : score_ctx {
        explicit Ctx(ScoreFunction&& wand, ScoreFunctions&& tail) noexcept
          : wand{std::move(wand)}, tail{std::move(tail)} {}

        ScoreFunction wand;
        ScoreFunctions tail;
      };

      return ScoreFunction::Make<Ctx>(
        [](score_ctx* ctx, score_t* res) noexcept {
          auto* scorers_ctx = static_cast<Ctx*>(ctx);
          IRS_ASSERT(res != nullptr);
          scorers_ctx->wand(res);
          for (auto& other : scorers_ctx->tail) {
            other.Score(++res);
          }
        },
        [](score_ctx* ctx, score_t arg) noexcept {
          auto* scorers_ctx = static_cast<Ctx*>(ctx);
          scorers_ctx->wand.Min(arg);
        },
        std::move(wand), std::move(tail));
    }
  }
}

ScoreFunction CompileScorers(ScoreFunctions&& scorers) {
  switch (scorers.size()) {
    case 0: {
      return ScoreFunction{};
    }
    case 1: {
      // The most important and frequent case when only
      // one scorer is provided.
      return std::move(scorers.front());
    }
    case 2: {
      struct Ctx final : score_ctx {
        Ctx(ScoreFunction&& func0, ScoreFunction&& func1) noexcept
          : func0{std::move(func0)}, func1{std::move(func1)} {}

        ScoreFunction func0;
        ScoreFunction func1;
      };

      return ScoreFunction::Make<Ctx>(
        [](score_ctx* ctx, score_t* res) noexcept {
          IRS_ASSERT(res != nullptr);
          auto* scorers_ctx = static_cast<Ctx*>(ctx);
          scorers_ctx->func0(res);
          scorers_ctx->func1(res + 1);
        },
        ScoreFunction::DefaultMin, std::move(scorers.front()),
        std::move(scorers.back()));
    }
    default: {
      struct Ctx final : score_ctx {
        explicit Ctx(ScoreFunctions&& scorers) noexcept
          : scorers{std::move(scorers)} {}

        ScoreFunctions scorers;
      };

      return ScoreFunction::Make<Ctx>(
        [](score_ctx* ctx, score_t* res) noexcept {
          auto& scorers = static_cast<Ctx*>(ctx)->scorers;
          for (auto& scorer : scorers) {
            scorer(res++);
          }
        },
        ScoreFunction::DefaultMin, std::move(scorers));
    }
  }
}

void PrepareCollectors(std::span<const ScorerBucket> order,
                       byte_type* stats_buf) {
  for (const auto& entry : order) {
    if (IRS_LIKELY(entry.bucket)) {
      entry.bucket->collect(stats_buf + entry.stats_offset, nullptr, nullptr);
    }
  }
}

void CompileScore(irs::score& score, std::span<const ScorerBucket> buckets,
                  const ColumnProvider& segment, const term_reader& field,
                  const byte_type* stats, const attribute_provider& doc,
                  score_t boost) {
  IRS_ASSERT(!buckets.empty());
  // wanderator could have score for first bucket and score upper bounds.
  if (score.IsDefault()) {
    auto scorers = PrepareScorers(buckets, segment, field, stats, doc, boost);
    // wanderator could have score upper bounds.
    if (score.max.tail == std::numeric_limits<score_t>::max()) {
      score.max.leaf = score.max.tail =
        scorers.empty() ? 0.f : scorers.front().Max();
    }
    score = CompileScorers(std::move(scorers));
  } else if (buckets.size() > 1) {
    auto scorers =
      PrepareScorers(buckets.subspan(1), segment, field, stats, doc, boost);
    score = CompileScorers(std::move(score), std::move(scorers));
    IRS_ASSERT(score.max.tail != std::numeric_limits<score_t>::max());
  }
  IRS_ASSERT(score.max.leaf <= score.max.tail);
}

}  // namespace irs
