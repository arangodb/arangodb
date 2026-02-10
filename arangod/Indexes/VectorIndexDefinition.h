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

/// @brief Scaling functions for document-count-dependent parameters.
enum class ScalingFunction : std::uint8_t {
  kSqrt,
  kLog,
  kCbrt,
  kLinear,
};

template<class Inspector>
inline auto inspect(Inspector& f, ScalingFunction& x) {
  return f.enumeration(x).values(
      ScalingFunction::kSqrt, "sqrt", ScalingFunction::kLog, "log",
      ScalingFunction::kCbrt, "cbrt", ScalingFunction::kLinear, "linear");
}

/// @brief A scaling specification: computes factor * func(N) where N is the
/// document count.
struct ScalingSpec {
  double factor{1.0};
  ScalingFunction function{ScalingFunction::kSqrt};

  bool operator==(ScalingSpec const&) const noexcept = default;

  std::int64_t compute(std::int64_t docCount) const {
    double raw;
    switch (function) {
      case ScalingFunction::kSqrt:
        raw = std::sqrt(static_cast<double>(docCount));
        break;
      case ScalingFunction::kLog:
        raw = std::log(static_cast<double>(docCount));
        break;
      case ScalingFunction::kCbrt:
        raw = std::cbrt(static_cast<double>(docCount));
        break;
      case ScalingFunction::kLinear:
        raw = static_cast<double>(docCount);
        break;
    }
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(factor * raw));
  }

  template<class Inspector>
  friend inline auto inspect(Inspector& f, ScalingSpec& x) {
    return f.object(x).fields(
        f.field("factor", x.factor)
            .invariant([](auto value) -> inspection::Status {
              if (value <= 0.0) {
                return {"factor must be greater than 0!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("function", x.function));
  }
};

/// @brief A parameter that is either a fixed integer or a scaling spec.
/// JSON: 100  OR  { "factor": 15, "function": "sqrt" }
using ScalableParameter = std::variant<std::int64_t, ScalingSpec>;

template<class Inspector>
inline auto inspect(Inspector& f, ScalableParameter& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::inlineType<std::int64_t>(), insp::inlineType<ScalingSpec>());
}

/// @brief Check if a ScalableParameter is in scaling mode.
inline bool isScaling(ScalableParameter const& p) {
  return std::holds_alternative<ScalingSpec>(p);
}

/// @brief Get the fixed value from a ScalableParameter.
/// Asserts that it is in fixed mode.
inline std::int64_t getFixed(ScalableParameter const& p) {
  return std::get<std::int64_t>(p);
}

/// @brief Get the ScalingSpec from a ScalableParameter.
/// Asserts that it is in scaling mode.
inline ScalingSpec const& getScaling(ScalableParameter const& p) {
  return std::get<ScalingSpec>(p);
}

/// @brief Resolve a ScalableParameter to a concrete value.
/// In fixed mode, returns the fixed value.
/// In scaling mode, computes factor * func(docCount).
inline std::int64_t resolveParameter(ScalableParameter const& p,
                                     std::int64_t docCount) {
  return std::visit(
      overload{
          [](std::int64_t fixed) -> std::int64_t { return fixed; },
          [docCount](ScalingSpec const& spec) -> std::int64_t {
            return spec.compute(docCount);
          },
      },
      p);
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
  ScalableParameter nLists;
  std::uint64_t trainingIterations;
  ScalableParameter defaultNProbe;

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
            .fallback(
                ScalableParameter{ScalingSpec{1.0, ScalingFunction::kSqrt}})
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
            .fallback(
                ScalableParameter{ScalingSpec{0.8, ScalingFunction::kSqrt}})
            .invariant([](auto const& value) -> inspection::Status {
              if (auto* fixed = std::get_if<std::int64_t>(&value)) {
                if (*fixed < 1) {
                  return {"defaultNProbe must be 1 or greater!"};
                }
              }
              return inspection::Status::Success{};
            }));
  }
};

}  // namespace arangodb
