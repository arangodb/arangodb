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

void KPathFinder::Ball::reset() {}

KPathFinder::KPathFinder(ShortestPathOptions& options) : _left{}, _right{} {}
KPathFinder::~KPathFinder() {}

void KPathFinder::reset() {
  _left.reset();
  _right.reset();
}

bool KPathFinder::hasMore() const { return false; }

// get the next available path serialized in the builder
bool KPathFinder::getNextPathAql(arangodb::velocypack::Builder& result) {
  return false;
}
