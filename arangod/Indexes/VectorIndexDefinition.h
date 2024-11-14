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

#include <vector>
#include <cstdint>

#include "Inspection/Status.h"
#include "fmt/format.h"

namespace arangodb {

// Number of training iterations, in faiss it is 25 by default
static constexpr int kdefaultTrainingIterations{25};

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
  std::int64_t numberOfCentroids;
  std::size_t codeSize;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, TrainedData& x) {
    return f.object(x).fields(f.field("codeData", x.codeData),
                              f.field("numberOfCentroids", x.numberOfCentroids),
                              f.field("codeSize", x.codeSize));
  }
};

struct UserVectorIndexDefinition {
  std::uint64_t dimension;
  SimilarityMetric metric;
  std::int64_t nLists;
  int trainingIterations;

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
        f.field("metric", x.metric), f.field("nLists", x.nLists),
        f.field("trainingIterations", x.trainingIterations)
            .fallback(kdefaultTrainingIterations));
  }
};

}  // namespace arangodb
