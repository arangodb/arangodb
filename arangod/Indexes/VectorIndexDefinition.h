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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <optional>
#include <cstdint>

#include "Inspection/Status.h"
#include "fmt/format.h"

namespace arangodb {

struct VectorHashFunction {
  double bParam;
  double wParam;
  std::vector<double> vParam;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, VectorHashFunction& x) {
    return f.object(x)
        .fields(f.field("bParam", x.bParam), f.field("wParam", x.wParam),
                f.field("vParam", x.vParam))
        .invariant([](VectorHashFunction& x) -> inspection::Status {
          if (x.wParam == 0) {
            return {"Division by zero is undefined!"};
          }

          return inspection::Status::Success{};
        });
  }
};

enum class SimilarityMetric : std::uint8_t {
  kL1,
  kL2,
  kCosine,
};

template<class Inspector>
inline auto inspect(Inspector& f, SimilarityMetric& x) {
  return f.enumeration(x).values(SimilarityMetric::kL1, "l1",
                                 SimilarityMetric::kL2, "l2",
                                 SimilarityMetric::kCosine, "cosine");
}

struct TrainedData {
  std::vector<std::uint8_t> codeData;
  std::size_t numberOfCodes;
  std::size_t codeSize;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, TrainedData& x) {
    return f.object(x).fields(f.field("codeData", x.codeData),
                              f.field("numberOfCodes", x.numberOfCodes),
                              f.field("codeSize", x.codeSize));
  }
};

struct UserVectorIndexDefinition {
  std::int64_t dimensions;
  SimilarityMetric metric;
  std::int64_t nLists;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, UserVectorIndexDefinition& x) {
    return f.object(x).fields(
        f.field("dimensions", x.dimensions)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"Dimensions must be greater then 0!"};
              }
              return inspection::Status::Success{};
            }),
        f.field("metric", x.metric).fallback(SimilarityMetric::kCosine),
        f.field("nLists", x.nLists));
  }
};

// TODO Extract Specific vector index params from general vector definition
struct FullVectorIndexDefinition : UserVectorIndexDefinition {
  std::optional<TrainedData> trainedData;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, FullVectorIndexDefinition& x) {
    return f.object(x).fields(
        f.template embedFields<UserVectorIndexDefinition&>(x),
        f.field("trainedData", x.trainedData));
  }
};

}  // namespace arangodb
