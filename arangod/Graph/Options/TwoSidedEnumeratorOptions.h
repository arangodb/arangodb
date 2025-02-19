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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/PathType.h"

#include <numeric>
#include <cstddef>

namespace arangodb {
namespace graph {

struct TwoSidedEnumeratorOptions {
 public:
  TwoSidedEnumeratorOptions(size_t minDepth, size_t maxDepth,
                            PathType::Type pathType);

  ~TwoSidedEnumeratorOptions();

  void setMinDepth(size_t min);
  void setMaxDepth(size_t max);
  [[nodiscard]] size_t getMinDepth() const;
  [[nodiscard]] size_t getMaxDepth() const;
  [[nodiscard]] PathType::Type getPathType() const;
  [[nodiscard]] bool getStopAtFirstDepth() const;
  [[nodiscard]] bool onlyProduceOnePath() const;

  void setStopAtFirstDepth(bool stopAtFirstDepth);
  void setOnlyProduceOnePath(bool onlyProduceOnePath);
  void setPathType(PathType::Type pathType) { _pathType = pathType; }

 private:
  size_t _minDepth;
  size_t _maxDepth;
  bool _stopAtFirstDepth{false};
  bool _onlyProduceOnePath{false};
  PathType::Type _pathType;
};
}  // namespace graph
}  // namespace arangodb
