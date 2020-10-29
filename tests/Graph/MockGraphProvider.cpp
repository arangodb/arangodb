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

#include "MockGraphProvider.h"

#include "./MockGraph.h"

#include "Futures/Future.h"
#include "Futures/Utilities.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;

MockGraphProvider::Step::Step(VertexType v)
    : vertex(v), previous(std::numeric_limits<size_t>::max()) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, EdgeType e)
    : vertex(v), previous(prev) {}
MockGraphProvider::Step::~Step() {}

MockGraphProvider::MockGraphProvider(MockGraph const& data) {
  for (auto const& it : data.edges()) {
    _fromIndex[it._from].push_back(it);
    _toIndex[it._to].push_back(it);
  }
}

MockGraphProvider::~MockGraphProvider() {}

auto MockGraphProvider::fetch(std::vector<Step> const& looseEnds) -> futures::Future<std::vector<Step>> {
  return futures::makeFuture(std::vector<Step>{});  
}

auto MockGraphProvider::expand(Step const& from, size_t previousIndex) -> std::vector<Step> {
  std::vector<Step> result{};
  if (_fromIndex.find(from.vertex) != _fromIndex.end()) {
    for (auto const& edge : _fromIndex[from.vertex]) {
      result.push_back(Step{previousIndex, edge._to, edge});
    }
  } 
  return result;
}


