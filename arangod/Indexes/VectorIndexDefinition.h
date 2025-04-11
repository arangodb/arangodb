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

#include <optional>
#include <vector>
#include <cstdint>

#include "Inspection/Status.h"
#include "fmt/format.h"

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

enum class SimilarityMetric : std::uint8_t {
  kL2,
  kCosine,
};

template<class Inspector>
inline auto inspect(Inspector& f, SimilarityMetric& x) {
  return f.enumeration(x).values(SimilarityMetric::kL2, "l2",
                                 SimilarityMetric::kCosine, "cosine");
}

struct TrainedData {
  std::vector<std::uint8_t> codeData;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, TrainedData& x) {
    return f.object(x).fields(f.field("codeData", x.codeData));
  }
};

struct UserVectorIndexDefinition {
  std::uint64_t dimension;
  SimilarityMetric metric;
  std::int64_t nLists;
  std::uint64_t trainingIterations;
  std::uint64_t defaultNProbe;

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
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"nLists must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("factory", x.factory),
        f.field("trainingIterations", x.trainingIterations)
            .fallback(kdefaultTrainingIterations),
        f.field("defaultNProbe", x.defaultNProbe)
            .fallback(kdefaultNProbe)
            .invariant([](auto value) -> inspection::Status {
              if (value == 0) {
                return {"defaultNProbe must be 1 or greater!"};
              }
              return inspection::Status::Success{};
            }));
  }
};

}  // namespace arangodb
