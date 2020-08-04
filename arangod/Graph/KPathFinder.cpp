////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "KPathFinder.h"

#include "Graph/ShortestPathOptions.h"

using namespace arangodb;
using namespace arangodb::graph;

KPathFinder::Ball::Ball() {}
KPathFinder::Ball::~Ball() = default;

void KPathFinder::Ball::reset(VertexRef center) {
  _center = center;
}

KPathFinder::KPathFinder(ShortestPathOptions& options) {}
KPathFinder::~KPathFinder() = default;

void KPathFinder::reset(VertexRef source, VertexRef target) {
  _left.reset(source);
  _right.reset(target);
  _done = false;
}

bool KPathFinder::hasMore() const { return !_done; }

// get the next available path serialized in the builder
bool KPathFinder::getNextPath(arangodb::velocypack::Builder& result) {
  _done = true;
  return false;
}
