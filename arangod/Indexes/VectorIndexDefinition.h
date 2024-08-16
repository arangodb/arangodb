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
  kEuclidian,
  kCosine,
};

template<class Inspector>
inline auto inspect(Inspector& f, SimilarityMetric& x) {
  return f.enumeration(x).values(SimilarityMetric::kEuclidian, "euclidian",
                                 SimilarityMetric::kCosine, "cosine");
}

// TODO Extract Specific vector index params from general vector definition
struct VectorIndexDefinition {
  std::int64_t dimensions;
  double min;
  double max;
  SimilarityMetric metric;
  std::int64_t nLists;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, VectorIndexDefinition& x) {
    return f.object(x)
        .fields(
            f.field("dimensions", x.dimensions), f.field("min", x.min),
            f.field("metric", x.metric).fallback(SimilarityMetric::kEuclidian),
            f.field("max", x.max), f.field("nLists", x.nLists))
        .invariant([](VectorIndexDefinition& x) -> inspection::Status {
          if (x.dimensions < 1) {
            return {"Dimensions must be greater then 0!"};
          }
          if (x.min > x.max) {
            return {"Min cannot be greater then max!"};
          }
          return inspection::Status::Success{};
        });
  }
};

}  // namespace arangodb
