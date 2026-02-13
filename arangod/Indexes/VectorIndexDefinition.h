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
#include <string_view>
#include <variant>
#include <vector>

#include "Basics/overload.h"
#include "Inspection/Status.h"
#include "Inspection/Types.h"

namespace arangodb {

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

/// @brief A single tier in the NLists scaling specification.
/// If the document count N >= minN, use fixedValue for nLists.
struct NListsTier {
  std::int64_t minN{0};
  std::int64_t fixedValue{1};

  bool operator==(NListsTier const&) const noexcept = default;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, NListsTier& x) {
    return f.object(x).fields(
        f.field("min_n", x.minN)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"min_n must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("fixed_value", x.fixedValue)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"fixed_value must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

/// @brief Tiered NLists scaling specification.
/// For small N: nLists = max(minNLists, multiplier * sqrt(N))
/// For large N: uses fixed values from tiers (first tier whose min_n <= N).
/// Tiers should be provided in descending order of min_n.
struct NListsScalingSpec {
  double multiplier{8.0};
  std::int64_t minNLists{10};
  std::vector<NListsTier> tiers;

  bool operator==(NListsScalingSpec const&) const noexcept = default;

  std::int64_t compute(std::int64_t docCount) const {
    // Sort tiers by minN descending to match the highest threshold first.
    auto sortedTiers = tiers;
    std::sort(sortedTiers.begin(), sortedTiers.end(),
              [](auto const& a, auto const& b) { return a.minN > b.minN; });
    for (auto const& tier : sortedTiers) {
      if (docCount >= tier.minN) {
        return tier.fixedValue;
      }
    }
    // No tier matched: use multiplier * sqrt(N), at least minNLists.
    auto raw = static_cast<std::int64_t>(
        multiplier * std::sqrt(static_cast<double>(docCount)));
    return std::max(minNLists, raw);
  }

  template<class Inspector>
  friend inline auto inspect(Inspector& f, NListsScalingSpec& x) {
    return f.object(x).fields(
        f.field("multiplier", x.multiplier)
            .invariant([](auto value) -> inspection::Status {
              if (value <= 0.0) {
                return {"multiplier must be greater than 0!"};
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
/// JSON: 100  OR  { "multiplier": 8, "minNLists": 32, "tiers": [...] }
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

/// @brief Compute default nProbe from resolved nLists: sqrt(nLists), min 1.
inline std::int64_t computeDefaultNProbe(std::int64_t resolvedNLists) {
  return std::max<std::int64_t>(1, static_cast<std::int64_t>(std::sqrt(
                                       static_cast<double>(resolvedNLists))));
}

/// @brief Resolve a scaling factory string by replacing {nLists} with the
/// computed nLists value.
/// Example: "IVF{nLists}_HNSW10,Flat" with nLists=1500
///       -> "IVF1500_HNSW10,Flat"
inline std::string resolveScalingFactory(std::string const& factoryTemplate,
                                         std::int64_t nLists) {
  std::string result = factoryTemplate;
  static constexpr std::string_view placeholder = "{nLists}";
  auto pos = result.find(placeholder);
  if (pos != std::string::npos) {
    result.replace(pos, placeholder.length(), std::to_string(nLists));
  }
  return result;
}

struct UserVectorIndexDefinition {
  std::uint64_t dimension;
  SimilarityMetric metric;
  NListsParameter nLists;
  std::uint64_t trainingIterations;

  // Optional explicit defaultNProbe override. When not set (nullopt),
  // defaultNProbe is computed as sqrt(resolvedNLists) with a minimum of 1.
  std::optional<std::int64_t> defaultNProbe;

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
            .fallback(NListsParameter{NListsScalingSpec{
                8.0,
                32,
                {{100000000, 1048576}, {10000000, 262144}, {1000000, 65536}}}})
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
            .invariant([](auto const& value) -> inspection::Status {
              if (value.has_value() && *value < 1) {
                return {"defaultNProbe must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

}  // namespace arangodb
