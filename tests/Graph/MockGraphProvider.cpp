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

#include "Basics/operating-system.h"

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"

#include "Futures/Future.h"
#include "Futures/Utilities.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;

MockGraphProvider::Step::Step(VertexType v)
    : vertex(v), previous(std::numeric_limits<size_t>::max()) {}

MockGraphProvider::Step::Step(size_t prev, VertexType v, EdgeType e)
    : vertex(v), previous(prev) {}
MockGraphProvider::Step::~Step() {}

void MockGraphProvider::Step::Vertex::addToBuilder(arangodb::velocypack::Builder& builder) const {
  std::string key = _vertex.toString();
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(key));
  builder.add(StaticStrings::IdString, VPackValue("v/" + key));
  builder.close();
}

void MockGraphProvider::Step::Edge::addToBuilder(arangodb::velocypack::Builder& builder) const {
  std::string fromId = "v/" + basics::StringUtils::itoa(_edge._from);
  std::string toId = "v/" + basics::StringUtils::itoa(_edge._to);
  std::string keyId = basics::StringUtils::itoa(_edge._from) + "->" + basics::StringUtils::itoa(_edge._to);

  builder.openObject();
  builder.add(StaticStrings::IdString, VPackValue("e/" + keyId));
  builder.add(StaticStrings::KeyString, VPackValue(keyId));
  builder.add(StaticStrings::FromString, VPackValue(fromId));
  builder.add(StaticStrings::ToString, VPackValue(toId));
  builder.add("weight", VPackValue(_edge._weight));
  builder.close();
}

arangodb::velocypack::HashedStringRef MockGraphProvider::Step::Vertex::getId() const {
  return _vertex;
}

MockGraphProvider::MockGraphProvider(MockGraph const& data, bool reverse) : _reverse(reverse) {
  for (auto const& it : data.edges()) {
    _fromIndex[it._from].push_back(it);
    _toIndex[it._to].push_back(it);
  }
}

MockGraphProvider::~MockGraphProvider() {}

auto MockGraphProvider::startVertex(VertexType v) -> Step { return Step(v); }

auto MockGraphProvider::fetch(std::vector<Step> const& looseEnds)
    -> futures::Future<std::vector<Step>> {
  return futures::makeFuture(std::vector<Step>{});
}

auto MockGraphProvider::expand(Step const& from, size_t previousIndex)
    -> std::vector<Step> {
  std::vector<Step> result{};
  if (_reverse) {
    if (_toIndex.find(from.vertex.data()) != _toIndex.end()) {
      for (auto const& edge : _toIndex[from.vertex.data()]) {
        result.push_back(Step{previousIndex, edge._from, edge});
      }
    }
  } else {
    if (_fromIndex.find(from.vertex.data()) != _fromIndex.end()) {
      for (auto const& edge : _fromIndex[from.vertex.data()]) {
        result.push_back(Step{previousIndex, edge._to, edge});
      }
    }
  }
  return result;
}
