////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Basics/overload.h"
#include "Inspection/Status.h"
#include "Inspection/Types.h"

namespace arangodb::vector {

// Number of training iterations, in faiss it is 25 by default
static constexpr std::uint64_t kdefaultTrainingIterations{25};
static constexpr std::uint64_t kdefaultNProbe{1};

struct SearchParameters {
  std::optional<std::int64_t> nProbe;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, SearchParameters& x) {
    return f.object(x).fields(
        f.field("nProbe", x.nProbe)
            .invariant([](auto value) -> inspection::Status {
              if (value.has_value() && *value < 1) {
                return {"nProbe must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

/// @brief Similarity metrics for vector index.
enum class SimilarityMetric : std::uint8_t {
  kL2,
  kCosine,
  kInnerProduct,
};

template<class Inspector>
inline auto inspect(Inspector& f, SimilarityMetric& x) {
  return f.enumeration(x).values(
      SimilarityMetric::kL2, "l2", SimilarityMetric::kCosine, "cosine",
      SimilarityMetric::kInnerProduct, "innerProduct");
}

struct TrainedData {
  std::vector<std::uint8_t> codeData;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, TrainedData& x) {
    return f.object(x).fields(f.field("codeData", x.codeData));
  }
};

/// @brief Strategy for computing nLists from document count.
enum class NListsStrategy : std::uint8_t {
  kAutoSqrt,
};

template<class Inspector>
inline auto inspect(Inspector& f, NListsStrategy& x) {
  return f.enumeration(x).values(NListsStrategy::kAutoSqrt, "autoSqrt");
}

/// @brief A single tier in the NLists scaling specification.
/// If the document count N >= threshold, use fixedValue for nLists.
struct NListsTier {
  std::int64_t threshold{0};
  std::int64_t fixedValue{1};

  bool operator==(NListsTier const&) const noexcept = default;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, NListsTier& x) {
    return f.object(x).fields(
        f.field("threshold", x.threshold)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"threshold must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("fixedValue", x.fixedValue)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"fixedValue must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

/// @brief Tiered NLists scaling specification.
/// For small N: nLists = max(minNLists, multiplier * func(N)), N < 1M
/// For large N: uses fixed values from tiers (first tier whose threshold <= N).
/// Tiers should be provided in descending order of threshold.
/// Values have been taken from autofaiss
/// {
///     strategy: "autoSqrt",
///     multiplier: 4,
///     minNLists: 2,
///     tiers: [
///         { treshold: 1_000_000,   fixedValue: 16384 },
///         { treshold: 10_000_000,  fixedValue: 65536 },
///         { treshold: 300_000_000, fixedValue: 131072 },
///     ],
/// }
/// so the rulse apply as such:
/// N < 1M: nLists = max(2, 4 * sqrt(N))
/// 1M <= N < 10M: nLists = 16384
/// 10M <= N < 300M: nLists = 65536
/// N >= 300M: nLists = 131072
struct NListsScalingSpec {
  NListsStrategy strategy{NListsStrategy::kAutoSqrt};
  std::int64_t multiplier{4};
  std::int64_t minNLists{2};
  std::vector<NListsTier> tiers{
      {1000000, 16384}, {10000000, 65536}, {300000000, 131072}};

  bool operator==(NListsScalingSpec const&) const noexcept = default;

  std::int64_t compute(std::int64_t docCount) const {
    auto sortedTiers = tiers;
    std::sort(
        sortedTiers.begin(), sortedTiers.end(),
        [](auto const& a, auto const& b) { return a.threshold > b.threshold; });
    for (auto const& tier : sortedTiers) {
      if (docCount >= tier.threshold) {
        return tier.fixedValue;
      }
    }

    // No tier matched: apply strategy.
    switch (strategy) {
      case NListsStrategy::kAutoSqrt: {
        return std::max(minNLists, static_cast<std::int64_t>(
                                       multiplier * std::sqrt(docCount)));
      }
    }
  }

  template<class Inspector>
  friend inline auto inspect(Inspector& f, NListsScalingSpec& x) {
    return f.object(x).fields(
        f.field("strategy", x.strategy).fallback(NListsStrategy::kAutoSqrt),
        f.field("multiplier", x.multiplier)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"multiplier must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("minNLists", x.minNLists)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"minNLists must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("tiers", x.tiers));
  }
};

/// @brief NLists parameter: either a fixed integer or a tiered scaling spec.
/// JSON: 100  OR  { "multiplier": 8, "minNLists": 10, "tiers": [...] }
using NListsParameter = std::variant<std::int64_t, NListsScalingSpec>;

template<class Inspector>
inline auto inspect(Inspector& f, NListsParameter& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::inlineType<std::int64_t>(), insp::inlineType<NListsScalingSpec>());
}

/// @brief Check if an NListsParameter is in scaling mode.
inline bool isNListsScaling(NListsParameter const& p) {
  return std::holds_alternative<NListsScalingSpec>(p);
}

/// @brief Resolve an NListsParameter to a concrete value.
/// In fixed mode, returns the fixed value.
/// In scaling mode, evaluates tiers or computes multiplier * sqrt(docCount).
inline std::int64_t resolveNListsParameter(NListsParameter const& p,
                                           std::int64_t docCount) {
  return std::visit(
      overload{
          [](std::int64_t fixed) -> std::int64_t { return fixed; },
          [docCount](NListsScalingSpec const& spec) -> std::int64_t {
            return spec.compute(docCount);
          },
      },
      p);
}

struct UserVectorIndexDefinition {
  std::uint64_t dimension;
  SimilarityMetric metric;
  NListsParameter nLists;
  std::uint64_t trainingIterations;

  std::int64_t defaultNProbe;

  // FAISS factory string. In fixed nLists mode, nLists must match the IVF
  // number in the string. In scaling nLists mode, the string must contain a
  // {nLists} placeholder that is resolved at training time.
  std::optional<std::string> factory;

  bool operator==(UserVectorIndexDefinition const&) const noexcept = default;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, UserVectorIndexDefinition& x) {
    return f.object(x).fields(
        f.field("dimension", x.dimension)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"Dimension must be greater than 0!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("metric", x.metric),
        f.field("nLists", x.nLists)
            .fallback(NListsParameter{NListsScalingSpec{}})
            .invariant([](auto const& value) -> inspection::Status {
              if (auto* fixed = std::get_if<std::int64_t>(&value)) {
                if (*fixed < 1) {
                  return {"nLists must be 1 or greater!"};
                }
              }
              return inspection::Status::Success{};
            }),
        f.field("factory", x.factory),
        f.field("trainingIterations", x.trainingIterations)
            .fallback(kdefaultTrainingIterations),
        f.field("defaultNProbe", x.defaultNProbe)
            .fallback(static_cast<std::int64_t>(kdefaultNProbe))
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"defaultNProbe must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

}  // namespace arangodb::vector
