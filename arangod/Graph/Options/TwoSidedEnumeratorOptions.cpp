////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "TwoSidedEnumeratorOptions.h"

using namespace arangodb;
using namespace arangodb::graph;

TwoSidedEnumeratorOptions::TwoSidedEnumeratorOptions(size_t minDepth,
                                                     size_t maxDepth,
                                                     PathType::Type pathType)
    : _minDepth(minDepth), _maxDepth(maxDepth), _pathType(pathType) {
  if (getPathType() == PathType::Type::AllShortestPaths) {
    setStopAtFirstDepth(true);
  } else if (getPathType() == PathType::Type::ShortestPath) {
    setOnlyProduceOnePath(true);
    setStopAtFirstDepth(false);
  }
}

TwoSidedEnumeratorOptions::~TwoSidedEnumeratorOptions() = default;

size_t TwoSidedEnumeratorOptions::getMinDepth() const { return _minDepth; }
size_t TwoSidedEnumeratorOptions::getMaxDepth() const { return _maxDepth; }

bool TwoSidedEnumeratorOptions::getStopAtFirstDepth() const {
  return _stopAtFirstDepth;
}
void TwoSidedEnumeratorOptions::setStopAtFirstDepth(bool stopAtFirstDepth) {
  _stopAtFirstDepth = stopAtFirstDepth;
}

void TwoSidedEnumeratorOptions::setOnlyProduceOnePath(bool onlyProduceOnePath) {
  _onlyProduceOnePath = onlyProduceOnePath;
}

bool TwoSidedEnumeratorOptions::onlyProduceOnePath() const {
  return _onlyProduceOnePath;
}

PathType::Type TwoSidedEnumeratorOptions::getPathType() const {
  return _pathType;
}