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

struct VectorIndexRandomVector {
  double bParam;
  double wParam;
  std::vector<double> Vparam;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, VectorIndexRandomVector& x) {
    return f.object(x)
        .fields(f.field("bParam", x.bParam), f.field("wParam", x.wParam),
                f.field("vParam", x.Vparam))
        .invariant([](VectorIndexRandomVector& x) -> inspection::Status {
          if (x.wParam == 0) {
            return {"Division by zero is undefined!"};
          }

          return inspection::Status::Success{};
        });
  }
};

struct VectorIndexDefinition {
  std::size_t dimensions;
  double min;
  double max;
  std::size_t Kparameter;
  std::size_t Lparameter;
  std::vector<VectorIndexRandomVector> randomFunctions;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, VectorIndexDefinition& x) {
    return f.object(x)
        .fields(f.field("dimensions", x.dimensions), f.field("min", x.min),
                f.field("max", x.max), f.field("Kparameter", x.Kparameter),
                f.field("Lparameter", x.Lparameter),
                f.field("randomFunctions", x.randomFunctions))
        .invariant([](VectorIndexDefinition& x) -> inspection::Status {
          if (x.dimensions < 1) {
            return {"Dimensions must be greater then 0!"};
          }
          if (x.Kparameter < 1) {
            return {"K parameter must be greater then 0!"};
          }
          if (x.Lparameter < 1 || x.Lparameter >= 256) {
            return {"L parameter must be greater then 0 and lower then 256!"};
          }
          if (x.min > x.max) {
            return {"Min cannot be greater then max!"};
          }
          if (x.randomFunctions.size() != x.Kparameter * x.Lparameter) {
            return fmt::format("Number of `randomFunctions` must be equal {}!",
                               x.Kparameter * x.Lparameter);
          }
          return inspection::Status::Success{};
        });
  }
};

}  // namespace arangodb
