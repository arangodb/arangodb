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
#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Assertions/Assert.h"
#include "Basics/overload.h"
#include "Inspection/Status.h"
#include "Inspection/Types.h"

namespace arangodb::vector {

// Number of training iterations, in faiss it is 25 by default
static constexpr std::uint64_t kdefaultTrainingIterations{25};
static constexpr std::uint64_t kdefaultNProbe{1};
// Matches autofaiss's points_per_cluster default; lower than FAISS's
// own max_points_per_centroid (256) to keep training memory in check.
static constexpr std::uint64_t kdefaultNumberOfDocsPerCentroid{100};

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
  std::size_t threshold;
  std::size_t fixedValue;

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
/// Values have been taken from autofaiss
/// {
///     strategy: "autoSqrt",
///     multiplier: 4,
///     minNLists: 2,
///     tiers: [
///         { threshold: 1_000_000,   fixedValue: 16384 },
///         { threshold: 10_000_000,  fixedValue: 65536 },
///         { threshold: 300_000_000, fixedValue: 131072 },
///     ],
/// }
/// So the rules apply as such:
/// N < 1M: nLists = max(2, 4 * sqrt(N))
/// 1M <= N < 10M: nLists = 16384
/// 10M <= N < 300M: nLists = 65536
/// N >= 300M: nLists = 131072
struct NListsScalingSpec {
  NListsStrategy strategy{NListsStrategy::kAutoSqrt};
  std::size_t multiplier{4};
  std::size_t minNLists{2};
  std::vector<NListsTier> tiers;

  bool operator==(NListsScalingSpec const&) const noexcept = default;

  std::size_t compute(std::size_t docCount) const {
    auto const it =
        std::ranges::max_element(tiers, {}, [&](NListsTier const& t) {
          return t.threshold <= docCount ? t.threshold : std::size_t{0};
        });
    if (it != tiers.end() && it->threshold <= docCount) {
      return it->fixedValue;
    }

    // No tier matched: apply strategy.
    switch (strategy) {
      case NListsStrategy::kAutoSqrt: {
        return std::max(minNLists, static_cast<std::size_t>(
                                       multiplier * std::sqrt(docCount)));
      }
    }
  }

  template<class Inspector>
  friend inline auto inspect(Inspector& f, NListsScalingSpec& x) {
    return f.object(x).fields(
        f.field("strategy", x.strategy),
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
        f.field("tiers", x.tiers)
            .fallback(std::vector<NListsTier>{
                {1000000, 16384}, {10000000, 65536}, {300000000, 131072}}));
  }
};

/// @brief NLists parameter: either a fixed integer or a tiered scaling spec.
using NListsParameter = std::variant<std::size_t, NListsScalingSpec>;

template<class Inspector>
inline auto inspect(Inspector& f, NListsParameter& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::inlineType<std::size_t>(), insp::inlineType<NListsScalingSpec>());
}

/// @brief Check if an NListsParameter is in scaling mode.
inline bool isNListsScaling(NListsParameter const& p) {
  return std::holds_alternative<NListsScalingSpec>(p);
}

/// @brief Resolve an NListsParameter to a concrete value.
/// In fixed mode, returns the fixed value.
/// In scaling mode, evaluates tiers or computes multiplier * sqrt(docCount).
inline std::size_t resolveNListsParameter(NListsParameter const& p,
                                          std::size_t docCount) {
  return std::visit(
      overload{
          [](std::size_t fixed) -> std::size_t { return fixed; },
          [docCount](NListsScalingSpec const& spec) -> std::size_t {
            return spec.compute(docCount);
          },
      },
      p);
}

/// @brief Check if an NListsParameter is in scaling mode.
inline bool isFactoryAStringScaling(std::string_view factoryString) {
  return factoryString.find("{}") != std::string_view::npos;
}

/// @brief Resolve an factory string to a concrete value.
/// In fixed mode, returne the string
/// In scaling mode, the factory string can be defined as temaplte
/// e.g. "IVF{}_HNSW32,SQ8" and the {} will be replaced by the resolved
/// nLists value
inline std::string resolveFactoryString(std::string const& factoryString,
                                        std::size_t nlists) {
  TRI_ASSERT(factoryString.find("{}") != std::string_view::npos);
  return std::vformat(factoryString, std::make_format_args(nlists));
}

struct UserVectorIndexDefinition {
  std::uint64_t dimension;
  SimilarityMetric metric;
  NListsParameter nLists;
  std::uint64_t trainingIterations;

  std::int64_t defaultNProbe;

  // Reservoir size = nLists * numberOfDocsPerCentroid * dimension *
  // sizeof(float). Lower this to reduce training memory at the cost of
  // training-set quality.
  std::uint64_t numberOfDocsPerCentroid;

  // FAISS factory string.
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
            .fallback(NListsParameter{NListsScalingSpec{
                NListsStrategy::kAutoSqrt,
                4,
                2,
                {{1000000, 16384}, {10000000, 65536}, {300000000, 131072}}}})
            .invariant([](NListsParameter const& value) -> inspection::Status {
              if (auto const* fixed = std::get_if<std::size_t>(&value)) {
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
            }),
        f.field("numberOfDocsPerCentroid", x.numberOfDocsPerCentroid)
            .fallback(kdefaultNumberOfDocsPerCentroid)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"numberOfDocsPerCentroid must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

}  // namespace arangodb::vector
